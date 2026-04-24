/*
 * 智能门锁 - 最终稳定版（内存优化）
 * 指纹：硬件串口(D0/D1)
 * 蓝牙：软串口(D8/D10)
 * 舵机：D9，蜂鸣器：D13
 * 键盘：行 A0~A3，列 D2~D5（实际接线列顺序为 D5,D4,D3,D2）
 * 上传前必须断开指纹模块VCC，上传后接回
 * 调试信息通过蓝牙发送至手机，无需打开串口监视器
 */

#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// ==================== 引脚定义 ====================
const int SERVO_PIN = 9;
const int BUZZER_PIN = 13;

// 键盘：行接 A0~A3，列接 D2~D5
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {14, 15, 16, 17};      // A0, A1, A2, A3
byte colPins[COLS] = {5, 4, 3, 2};          // 

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
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
  delay(2000);
  setServo(0);
  debugPrintln(F("🔒 门锁已关闭"));
  wrongCount = 0;
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

// ==================== 密码验证 ====================
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
  debugPrintln(F("   智能门锁系统 v3.0 (硬件串口指纹)"));
  debugPrintln(F("======================================="));
  debugPrintln(F("开锁方式："));
  debugPrintln(F("  1. 密码：输入密码，然后按 #"));
  debugPrintln(F("  2. 指纹：按压已录入的手指"));
  debugPrintln(F("  3. 蓝牙：发送 O 或 open"));
  debugPrintln(F("======================================\n"));
  
  beepShort();
}

void loop() {
  updateLock();
  checkPassword();
  checkFingerprint();
  checkBluetooth();
}
