/*
 * 智能门锁完整程序（软串口版）
 * 功能：指纹(D6/D7) + 键盘密码 + 蓝牙(D8/D10) 开锁
 * 舵机(D9)，蜂鸣器(D13)
 */

#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// ==================== 引脚定义 ====================
const int SERVO_PIN = 9;
const int BUZZER_PIN = 13;

// 键盘引脚
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {14, 15, 16, 17};  // A0=14, A1=15, A2=16, A3=17

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// 指纹模块（软串口 D6/D7）
SoftwareSerial fingerSerial(6, 7);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// 蓝牙模块（软串口 D8/D10）
SoftwareSerial bluetooth(8, 10);

// 密码设置
const String CORRECT_PASSWORD = "123456";
const int MAX_ATTEMPTS = 3;
const unsigned long LOCKOUT_TIME = 300000;  // 5分钟

String inputPassword = "";
int wrongAttempts = 0;
unsigned long lockoutStartTime = 0;
bool isLocked = false;

// ==================== 舵机控制（手动PWM） ====================
void setServoAngle(int angle) {
  int pulseWidth = map(angle, 0, 180, 500, 2480);
  for (int i = 0; i < 50; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(20000 - pulseWidth);
  }
}

void initServo() {
  pinMode(SERVO_PIN, OUTPUT);
  setServoAngle(0);
  Serial.println(F("舵机初始化完成（0度）"));
}

// ==================== 蜂鸣器 ====================
void buzzerInit() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepShort() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepLong() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepDouble() {
  beepShort();
  delay(100);
  beepShort();
}

// ==================== 开锁动作 ====================
void unlockDoor() {
  Serial.println(F("✅ 验证成功！门锁已打开！"));
  beepShort();
  setServoAngle(90);
  delay(2000);
  setServoAngle(0);
  Serial.println(F("🔒 门锁已关闭"));
  wrongAttempts = 0;
}

// ==================== 锁定逻辑 ====================
void checkLockout() {
  if (wrongAttempts >= MAX_ATTEMPTS) {
    isLocked = true;
    lockoutStartTime = millis();
    Serial.println(F("⛔ 错误次数过多，系统已锁定5分钟！"));
    beepDouble();
  }
}

void updateLockout() {
  if (isLocked && (millis() - lockoutStartTime >= LOCKOUT_TIME)) {
    isLocked = false;
    wrongAttempts = 0;
    Serial.println(F("🔓 锁定时间结束，系统已解锁"));
    beepShort();
  }
}

// ==================== 密码验证 ====================
void checkPassword() {
  if (isLocked) return;
  
  char key = keypad.getKey();
  if (key) {
    Serial.print(key);
    beepShort();
    
    if (key == '#') {
      Serial.println();
      Serial.print(F("输入的密码是："));
      Serial.println(inputPassword);
      if (inputPassword == CORRECT_PASSWORD) {
        unlockDoor();
        inputPassword = "";
      } else {
        Serial.println(F("❌ 密码错误，请重试！"));
        beepLong();
        wrongAttempts++;
        Serial.print(F("错误次数："));
        Serial.println(wrongAttempts);
        inputPassword = "";
        checkLockout();
      }
    }
    else if (key == '*') {
      inputPassword = "";
      Serial.println(F(" (已清除)"));
    }
    else {
      if (inputPassword.length() < 6) {
        inputPassword += key;
        if (inputPassword.length() == 6) {
          Serial.println();
          Serial.println(F("已输入6位密码，请按#确认"));
        }
      } else {
        Serial.println();
        Serial.println(F("⚠️ 已输入6位，如需修改请按*清除后重新输入"));
      }
    }
  }
}

// ==================== 指纹验证 ====================
void checkFingerprint() {
  if (isLocked) return;
  
  uint8_t result = finger.getImage();
  if (result != FINGERPRINT_OK) return;
  
  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) {
    Serial.println(F("⚠️ 指纹图像无效，请重新按压"));
    return;
  }
  
  result = finger.fingerSearch();
  if (result == FINGERPRINT_OK) {
    Serial.print(F("🔓 指纹验证成功！用户ID："));
    Serial.println(finger.fingerID);
    unlockDoor();
  } else if (result == FINGERPRINT_NOTFOUND) {
    Serial.println(F("❌ 指纹验证失败，未找到匹配指纹"));
    beepLong();
    wrongAttempts++;
    Serial.print(F("错误次数："));
    Serial.println(wrongAttempts);
    checkLockout();
  } else {
    Serial.println(F("⚠️ 指纹识别出错，请重试"));
  }
}

// ==================== 蓝牙指令 ====================
void checkBluetooth() {
  if (bluetooth.available()) {
    // 累积接收所有字节，直到100ms内没有新数据
    String cmd = "";
    unsigned long lastTime = millis();
    while (millis() - lastTime < 100) {
      if (bluetooth.available()) {
        char c = bluetooth.read();
        cmd += c;
        lastTime = millis();  // 每收到一个字节就重置计时
      }
    }
    cmd.trim();  // 去掉首尾的空白字符（换行、回车、空格）
    
    // 统一转为小写比较（兼容大小写）
    cmd.toLowerCase();
    
    // 打印收到的指令（带换行，方便调试）
    Serial.print(F("蓝牙收到: ["));
    Serial.print(cmd);
    Serial.println(F("]"));
    
    if (cmd == "o" || cmd == "open") {
      Serial.println(F("📱 收到蓝牙开锁指令"));
      unlockDoor();
    }
  }
}

// ==================== 指纹模块初始化 ====================
void initFingerprint() {
  Serial.println(F("正在初始化指纹模块（软串口 D6/D7）..."));
  // 根据你的模块实际波特率修改（常见57600或9600）
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println(F("✅ 指纹模块连接成功！"));
    finger.getTemplateCount();
    Serial.print(F("📌 当前指纹库中有 "));
    Serial.print(finger.templateCount);
    Serial.println(F(" 个指纹"));
  } else {
    Serial.println(F("❌ 指纹模块连接失败！"));
    Serial.println(F("请检查：1. 接线 TX→D6, RX→D7  2. 独立供电且共地  3. 波特率"));
    while (1);
  }
}

// ==================== 初始化 ====================
void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(100);
  
  bluetooth.begin(9600);
  buzzerInit();
  initServo();
  initFingerprint();
  
  Serial.println(F("\n========================================"));
  Serial.println(F("   指纹密码蓝牙智能门锁系统 v3.0"));
  Serial.println(F("========================================"));
  Serial.println(F("开锁方式："));
  Serial.println(F("  1. 密码：123456 然后按 #"));
  Serial.println(F("  2. 指纹：按压已录入的手指"));
  Serial.println(F("  3. 蓝牙：发送 O 或 open"));
  Serial.println(F("按 * 清除密码，按 # 确认"));
  Serial.println(F("========================================\n"));
  
  beepShort();
}

void loop() {
  updateLockout();
  checkPassword();
  checkFingerprint();
  checkBluetooth();
}