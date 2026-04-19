/*
 * 指纹删除程序 - 改进版
 * 解决了 "ID 不匹配" 的问题
 */

#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

SoftwareSerial fingerSerial(6, 7);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(100);

  Serial.println(F("\n\n指纹删除程序 (改进版)"));
  finger.begin(57600); // 如果连接失败，改为9600试试

  if (finger.verifyPassword()) {
    Serial.println(F("指纹模块连接成功"));
    finger.getTemplateCount();
    Serial.print(F("当前指纹库数量: "));
    Serial.println(finger.templateCount);
  } else {
    Serial.println(F("指纹模块连接失败！请检查接线"));
    while (1);
  }

  Serial.println(F("\n请输入要删除的指纹ID (1-127)"));
}

// 从串口读取整行的字符串，并将其转换为整数
int readIntFromSerial() {
  while (!Serial.available()); // 等待数据
  String input = Serial.readStringUntil('\n'); // 读取整行直到换行符
  input.trim(); // 去掉首尾的空白字符（如空格、换行、回车）
  return input.toInt(); // 转换为整数
}

void loop() {
  int id = readIntFromSerial(); // 调用新函数获取ID

  if (id >= 1 && id <= 127) {
    Serial.print(F("你输入了ID: "));
    Serial.println(id);
    Serial.println(F("请再次输入相同ID以确认删除:"));

    int confirmId = readIntFromSerial();

    if (id == confirmId) {
      Serial.print(F("正在删除 ID #"));
      Serial.print(id);
      Serial.println(F(" ..."));
      uint8_t result = finger.deleteModel(id);
      if (result == FINGERPRINT_OK) {
        Serial.println(F("✅ 指纹删除成功！"));
      } else {
        Serial.println(F("❌ 删除失败，可能ID不存在或硬件错误"));
      }
    } else {
      Serial.println(F("❌ ID不匹配，已取消删除操作"));
    }
  } else {
    Serial.println(F("ID无效，请输入1-127之间的数字"));
  }
  Serial.println(F("\n请输入要删除的指纹ID (1-127)"));
}