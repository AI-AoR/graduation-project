/*
 * 蓝牙模块 HC-05 测试代码
 * 功能：手机蓝牙串口发送指令 -> 控制舵机开锁
 * 舵机接 D9，蜂鸣器接 D13
 * 蓝牙模块：D8(RX), D10(TX)
 */

#include <SoftwareSerial.h>

// 引脚定义
const int SERVO_PIN = 9;
const int BUZZER_PIN = 13;

// 蓝牙软串口
SoftwareSerial bluetooth(8, 10);  // RX = 8, TX = 10

// 舵机控制（手动PWM，不用Servo库）
void setServoAngle(int angle) {
  // angle: 0~180度
  int pulseWidth = map(angle, 0, 180, 500, 2480);  // 脉宽500~2480微秒
  for (int i = 0; i < 50; i++) {   // 发送50个脉冲，确保舵机转到位置
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(20000 - pulseWidth);
  }
}

// 蜂鸣器短响
void beep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

// 开锁动作
void unlockDoor() {
  Serial.println("开锁指令已接收，已开锁...");
  beep();
  setServoAngle(90);   // 转到90度（开锁位置）
  delay(2000);         // 保持2秒
  setServoAngle(0);    // 回到0度（锁门位置）
  Serial.println("门锁已关闭");
}

void setup() {
  Serial.begin(9600);          // 串口监视器
  bluetooth.begin(9600);       // 蓝牙模块波特率（HC-05默认9600）
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // 舵机初始化，转到0度
  setServoAngle(0);
  
  Serial.println("=================================");
  Serial.println("  蓝牙舵机测试程序已启动");
  Serial.println("=================================");
  Serial.println("请用手机蓝牙串口工具连接 HC-05");
  Serial.println("发送指令：");
  Serial.println("  O 或 open  -> 开锁");
  Serial.println("=================================");
}

void loop() {
  // 检查蓝牙是否有数据
  if (bluetooth.available()) {
    String cmd = bluetooth.readString();
    cmd.trim();   // 去掉回车换行
    Serial.print("收到指令：");
    Serial.println(cmd);
    
    if (cmd == "O" || cmd == "open") {
      unlockDoor();
    } else {
      Serial.println("未知指令，请发送 O 或 open");
    }
  }
  
  // 可选：也支持从串口监视器输入指令（方便调试）
  if (Serial.available()) {
    String cmd = Serial.readString();
    cmd.trim();
    if (cmd == "O" || cmd == "open") {
      unlockDoor();
    }
  }
}