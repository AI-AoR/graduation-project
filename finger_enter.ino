/*
 * 指纹录入程序 - 使用矩阵键盘控制
 * 功能：键盘输入ID，*确认录入，#退出
 * 接线：键盘 D2-D5, A0-A3；指纹 D6,D7
 */

#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// ==================== 键盘配置 ====================
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

// ==================== 指纹模块（软串口） ====================
SoftwareSerial fingerSerial(6, 7);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// 蜂鸣器（可选，用于提示）
const int BUZZER_PIN = 13;

// 状态变量
int inputId = 0;          // 当前输入的ID
bool waitingForId = true; // 等待输入ID
bool keepRunning = true;  // 是否继续运行

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println(F("\n\n========================================"));
  Serial.println(F("   指纹录入程序 - 键盘控制版"));
  Serial.println(F("========================================"));
  Serial.println(F("操作说明："));
  Serial.println(F("  1. 使用数字键输入ID (1-127)"));
  Serial.println(F("  2. 按 * 键确认录入"));
  Serial.println(F("  3. 按提示按压手指两次"));
  Serial.println(F("  4. 录入成功后，按 * 继续，按 # 退出"));
  Serial.println(F("========================================\n"));
  
  // 初始化指纹模块
  finger.begin(57600);   // 如果失败，改为9600
  if (finger.verifyPassword()) {
    Serial.println(F("✅ 指纹模块连接成功"));
    finger.getTemplateCount();
    Serial.print(F("当前指纹库数量: "));
    Serial.println(finger.templateCount);
  } else {
    Serial.println(F("❌ 指纹模块连接失败！请检查接线"));
    while (1);
  }
  
  beepShort();
  Serial.println(F("\n请输入指纹ID (1-127)，然后按 * 确认"));
}

void loop() {
  if (!keepRunning) return;
  
  if (waitingForId) {
    // 等待键盘输入ID
    char key = keypad.getKey();
    if (key) {
      beepShort();
      if (key >= '0' && key <= '9') {
        // 数字键：累加输入
        int digit = key - '0';
        int newId = inputId * 10 + digit;
        if (newId <= 127) {
          inputId = newId;
          Serial.print(digit);
        } else {
          Serial.println(F("\n⚠️ ID不能超过127，请重新输入"));
          inputId = 0;
          Serial.print(F("请输入ID: "));
        }
      }
      else if (key == '*') {
        // 确认ID
        if (inputId >= 1 && inputId <= 127) {
          Serial.println();
          Serial.print(F("准备录入 ID: "));
          Serial.println(inputId);
          waitingForId = false;
          // 开始录入指纹
          bool success = enrollFingerprint(inputId);
          if (success) {
            Serial.println(F("\n✅ 指纹录入成功！"));
            beepShort();
            // 询问是否继续
            Serial.println(F("\n按 * 继续录入，按 # 退出"));
            // 等待键盘选择
            while (true) {
              char choice = keypad.getKey();
              if (choice == '*') {
                // 继续录入
                Serial.println(F("\n继续录入下一个指纹...\n"));
                inputId = 0;
                waitingForId = true;
                Serial.print(F("请输入新的ID: "));
                break;
              } else if (choice == '#') {
                // 退出程序
                keepRunning = false;
                Serial.println(F("\n录入程序结束，你可以上传主程序了。"));
                break;
              }
            }
          } else {
            Serial.println(F("\n❌ 录入失败，请重试"));
            inputId = 0;
            waitingForId = true;
            Serial.print(F("请重新输入ID: "));
          }
        } else {
          Serial.println(F("\n⚠️ ID无效，请输入1-127的数字"));
          inputId = 0;
          Serial.print(F("请输入ID: "));
        }
      }
      else if (key == '#') {
        // 直接退出
        keepRunning = false;
        Serial.println(F("\n用户主动退出录入程序。"));
      }
      else {
        // 其他按键无效
        Serial.println(F("\n⚠️ 无效按键，请输入数字、*或#"));
        Serial.print(F("请输入ID: "));
      }
    }
  }
}

// 录入指纹核心函数
bool enrollFingerprint(uint8_t id) {
  int p = -1;
  Serial.print(F("请按手指到传感器上..."));
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println(F(" 图像获取成功"));
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(F("."));
        delay(500);  // 避免刷屏太快
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println(F(" 通信错误"));
        return false;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println(F(" 成像错误"));
        return false;
      default:
        Serial.println(F(" 未知错误"));
        return false;
    }
  }
  
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println(F(" 图像特征转换失败"));
    return false;
  }
  
  Serial.println(F("请移开手指..."));
  delay(2000);
  
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  
  Serial.print(F("请再次按同一手指..."));
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println(F(" 图像获取成功"));
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(F("."));
        delay(500);
        break;
      default:
        Serial.println(F(""));
        return false;
    }
  }
  
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println(F(" 图像特征转换失败"));
    return false;
  }
  
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println(F(" 指纹模型创建失败"));
    return false;
  }
  
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.println(F(" 指纹存储失败"));
    return false;
  }
  
  Serial.println();
  Serial.print(F("✅ ID #"));
  Serial.print(id);
  Serial.println(F(" 录入成功！"));
  return true;
}

// 蜂鸣器短响
void beepShort() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}