/*
 * 智能门锁 - 最终稳定版（内存优化） + 手动关锁 + 指纹录入
 * 指纹：硬件串口(D0/D1)
 * 蓝牙：软串口(D8/D10)
 * 舵机：D9，蜂鸣器：D13
 * 键盘：行 A0~A3，列 D2~D5（实际接线列顺序为 D5,D4,D3,D2）
 * 手动关锁：键盘按 D
 * 指纹录入：上电后5秒内按 *
 * 上传前必须断开指纹模块VCC，上传后接回
 * 调试信息通过蓝牙发送至手机，无需打开串口监视器
 */

#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// ==================== 引脚定义 ====================
const int SERVO_PIN = 9;
const int BUZZER_PIN = 13;

// ==================== 变量定义 ====================
bool isOpen = false;          // 门是否处于打开状态
unsigned long openStartTime;  // 开锁开始的时间

// 键盘：行接 A0~A3，列接 D2~D5
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {14, 15, 16, 17};      // A0, A1, A2, A3
byte colPins[COLS] = {5, 4, 3, 2};          // 列顺序 D5,D4,D3,D2

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}          // D键用于手动关锁
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// 指纹模块（硬件串口）
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial);

// 蓝牙模块（软串口）
SoftwareSerial bluetooth(8, 10);

// 密码及锁定
const String PASSWORD = "123456";
const int MAX_ATTEMPTS = 3;
const unsigned long LOCKOUT_TIME = 300000;
String inputPwd = "";
int wrongCount = 0;
unsigned long lockStart = 0;
bool locked = false;

// ==================== 调试输出（通过蓝牙发送到手机） ====================
void debugPrint(const __FlashStringHelper* msg) {
  bluetooth.print(msg);
}
void debugPrintln(const __FlashStringHelper* msg) {
  bluetooth.println(msg);
}
void debugPrint(String msg) {
  bluetooth.print(msg);
}
void debugPrintln(String msg) {
  bluetooth.println(msg);
}
void debugPrint(char c) {
  bluetooth.print(c);
}

// ==================== 舵机控制（手动PWM） ====================
void setServo(int angle) {
  int pw = map(angle, 0, 180, 500, 2480);
  for (int i = 0; i < 50; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pw);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(20000 - pw);
  }
}
void initServo() {
  pinMode(SERVO_PIN, OUTPUT);
  setServo(0);
}

// ==================== 蜂鸣器 ====================
void beepShort() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(80);
  digitalWrite(BUZZER_PIN, LOW);
}
void beepLong() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(400);
  digitalWrite(BUZZER_PIN, LOW);
}
void beepDouble() {
  beepShort(); delay(100); beepShort();
}

// ==================== 开锁 ====================
void unlock() {
  debugPrintln(F("✅ 验证成功！门锁已打开！"));
  beepShort();
  setServo(90);
  isOpen = true;                      // 标记门已打开
  openStartTime = millis();           // 记录开锁时间
  wrongCount = 0;
}

// ==================== 手动关锁 ====================
void lockManually() {
  if (!isOpen) return;          // 门没开就不用关
  setServo(0);
  isOpen = false;
  openStartTime = 0;            // 重置计时（关键）
  debugPrintln(F("🔒 手动关锁完成"));
}

// ==================== 锁定逻辑 ====================
void checkLock() {
  if (wrongCount >= MAX_ATTEMPTS) {
    locked = true;
    lockStart = millis();
    debugPrintln(F("⛔ 错误次数过多，系统已锁定5分钟！"));
    beepDouble();
  }
}
void updateLock() {
  if (locked && (millis() - lockStart >= LOCKOUT_TIME)) {
    locked = false;
    wrongCount = 0;
    debugPrintln(F("🔓 锁定时间结束，系统已解锁"));
    beepShort();
  }
}

// ==================== 密码验证 + 手动关锁 ====================
void checkPassword() {
  if (locked) return;
  char key = keypad.getKey();
  if (!key) return;
  
  debugPrint(key);
  beepShort();

  if (key == '#') {
    debugPrintln("");
    debugPrint(F("输入的密码："));
    debugPrintln(inputPwd);
    if (inputPwd == PASSWORD) {
      unlock();
    } else {
      debugPrintln(F("❌ 密码错误！"));
      beepLong();
      wrongCount++;
      checkLock();
    }
    inputPwd = "";
  } else if (key == '*') {
    inputPwd = "";
    debugPrintln(F(" (已清除)"));
  } else if (key == 'D') {
    // 手动关锁
    lockManually();
  } else {
    if (inputPwd.length() < 6) {
      inputPwd += key;
      if (inputPwd.length() == 6) {
        debugPrintln(F(" (已输入6位，请按#确认)"));
      }
    } else {
      debugPrintln(F(" (已达6位，请按#或*)"));
    }
  }
}

// ==================== 指纹验证 ====================
void checkFingerprint() {
  if (locked) return;
  uint8_t r = finger.getImage();
  if (r != FINGERPRINT_OK) return;
  
  r = finger.image2Tz();
  if (r != FINGERPRINT_OK) {
    debugPrintln(F("⚠️ 指纹图像无效"));
    return;
  }
  
  r = finger.fingerSearch();
  if (r == FINGERPRINT_OK) {
    debugPrint(F("🔓 指纹验证成功！用户ID："));
    debugPrintln(String(finger.fingerID));
    unlock();
  } else if (r == FINGERPRINT_NOTFOUND) {
    debugPrintln(F("❌ 指纹验证失败"));
    beepLong();
    wrongCount++;
    checkLock();
  } else {
    debugPrintln(F("⚠️ 识别错误"));
  }
}

// ==================== 蓝牙指令 ====================
void checkBluetooth() {
  if (bluetooth.available()) {
    delay(80);
    String cmd = "";
    while (bluetooth.available()) {
      cmd += (char)bluetooth.read();
    }
    cmd.trim();
    cmd.toLowerCase();
    
    // 过滤纯数字（避免显示键盘输入的回显）
    bool isAllDigits = true;
    for (char c : cmd) {
      if (!isDigit(c)) { isAllDigits = false; break; }
    }
    if (cmd.length() == 0 || isAllDigits) return;
    
    if (cmd == "o" || cmd == "open") {
      debugPrintln(F("📱 收到蓝牙开锁指令"));
      unlock();
    } else {
      debugPrint(F("未知指令: "));
      debugPrintln(cmd);
    }
  }
}

// ==================== 指纹录入函数 ====================
void enrollFingerprint() {
  uint8_t id = 1;
  debugPrintln(F("\n进入指纹录入模式"));
  debugPrintln(F("请输入ID号(1-127)，后按#确认；输入*取消"));
  
  // 等待输入ID
  String idStr = "";
  while (true) {
    char key = keypad.getKey();
    if (key) {
      delay(50);
      if (key >= '0' && key <= '9') {
        idStr += key;
        debugPrint(key);
      } else if (key == '#') {
        if (idStr.length() > 0) {
          id = idStr.toInt();
          if (id >= 1 && id <= 127) break;
          else {
            debugPrintln(F("\nID范围1-127，请重新输入"));
            idStr = "";
          }
        } else {
          debugPrintln(F("\nID不能为空"));
        }
      } else if (key == '*') {
        debugPrintln(F("\n取消录入"));
        return;
      }
    }
  }
  debugPrintln(F("\n开始录入指纹..."));
  
  // 采集第一次
  while (finger.getImage() != FINGERPRINT_OK) {
    debugPrintln(F("请按压手指"));
    delay(500);
  }
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    debugPrintln(F("图像错误，请重试"));
    return;
  }
  debugPrintln(F("移开手指"));
  delay(1500);
  
  // 采集第二次
  while (finger.getImage() != FINGERPRINT_OK) {
    debugPrintln(F("再次按压同一手指"));
    delay(500);
  }
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    debugPrintln(F("图像错误，请重试"));
    return;
  }
  
  // 创建模型
  if (finger.createModel() != FINGERPRINT_OK) {
    debugPrintln(F("指纹不匹配，请重试"));
    return;
  }
  
  // 存储
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    debugPrintln(F("存储失败"));
    return;
  }
  
  debugPrintln(F("✅ 指纹录入成功！"));
  beepShort();
}

// ==================== 指纹初始化 ====================
void initFingerprint() {
  debugPrintln(F("正在初始化指纹模块..."));
  finger.begin(57600);  
  if (finger.verifyPassword()) {
    debugPrintln(F("✅ 指纹模块连接成功"));
    finger.getTemplateCount();
    debugPrint(F("📌 指纹库数量: "));
    debugPrintln(String(finger.templateCount));
  } else {
    debugPrintln(F("❌ 指纹模块连接失败！请检查接线和波特率"));
    while (1) { delay(1000); }
  }
}

// ==================== 初始化 ====================
void setup() {
  Serial.begin(9600);           // 硬件串口给指纹
  bluetooth.begin(9600);        // 蓝牙软串口
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  initServo();
  initFingerprint();
  
  debugPrintln(F("\n======================================"));
  debugPrintln(F("   智能门锁系统 v3.1 (手动关锁+录入)"));
  debugPrintln(F("======================================="));
  debugPrintln(F("开锁方式："));
  debugPrintln(F("  1. 密码：输入密码，然后按 #"));
  debugPrintln(F("  2. 指纹：按压已录入的手指"));
  debugPrintln(F("  3. 蓝牙：发送 O 或 open"));
  debugPrintln(F("手动关锁：按 D 键"));
  debugPrintln(F("指纹录入：上电后5秒内按 * 键"));
  debugPrintln(F("======================================\n"));
  
  beepShort();
  
  // 等待5秒，按 * 进入指纹录入模式
  debugPrintln(F("5秒内按 * 进入指纹录入模式..."));
  unsigned long start = millis();
  bool enterEnroll = false;
  while (millis() - start < 5000) {
    char key = keypad.getKey();
    if (key == '*') {
      enterEnroll = true;
      break;
    }
  }
  if (enterEnroll) {
    debugPrintln(F("进入文件录入模式"));
    while (true) {
      enrollFingerprint();
      debugPrintln(F("按 * 继续录入，按 # 退出"));
      char c = 0;
      while (!c) c = keypad.getKey();
      if (c == '#') break;
      else if (c == '*') continue;
    }
    debugPrintln(F("退出录入模式，系统正常启动"));
  }
}

void loop() {
  updateLock();           // 原有的锁定更新
  checkPassword();        // 原有的密码检测（也包括 D 键处理）
  checkFingerprint();
  checkBluetooth();
  // 处理开锁后的自动关锁
  if (isOpen && (millis() - openStartTime >= 5000)) {
    setServo(0);
    isOpen = false;
    debugPrintln(F("🔒 门锁已自动关闭"));
  }
}
