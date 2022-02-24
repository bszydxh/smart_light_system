/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/
#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "SPI.h"  //U8g2.h依赖 Blinker.h依赖
#include "Wire.h" //U8g2.h依赖
#include "U8g2lib.h"
#include "WiFi.h"             //Blinker.h依赖
#include "ESPmDNS.h"          //Blinker.h依赖
#include "FS.h"               //Blinker.h依赖
#include "SPIFFS.h"           //Blinker.h依赖
#include "Ticker.h"           //Blinker.h依赖
#include "Update.h"           //Blinker.h依赖
#include "WiFiClientSecure.h" //Blinker.h依赖
#include "EEPROM.h"           //Blinker.h依赖
#include "BLEScan.h"
#define BLINKER_PRINT Serial // Blinker.h依赖
#define BLINKER_WIFI         // Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#include "HTTPClient.h"
#include "Blinker.h"
#include "FastLED.h"
#define NUM_LEDS 120
#define DATA_PIN 25
#define USE_MULTCORE 0
#include "WiFiUdp.h"
#define ssid "324-右"          //这里改成你的设备当前环境下WIFI名字
#define password "21009200835" //这里改成你的设备当前环境下WIFI密码

WiFiUDP Udp;                       //实例化WiFiUDP对象
unsigned int localUdpPort = 1234;  // 自定义本地监听端口
unsigned int remoteUdpPort = 4321; // 自定义远程监听端口
char incomingPacket[255];          // 保存Udp工具发过来的消息
char replyPacket[] = "turn_off";   //发送的消息,仅支持英文

void setup()
{
  Serial.begin(115200); //打开串口
  Serial.println();

  Serial.printf("正在连接 %s ", ssid);
  WiFi.begin(ssid, password);           //连接到wifi
  while (WiFi.status() != WL_CONNECTED) //等待连接
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("连接成功");

  if (Udp.begin(localUdpPort))
  { //启动Udp监听服务
    Serial.println("监听成功");

    //打印本地的ip地址，在UDP工具中会使用到
    // WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
    Serial.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), localUdpPort);
  }
  else
  {
    Serial.println("监听失败");
  }
}

void loop()
{
  //向udp工具发送消息
  Udp.beginPacket("255.255.255.255", remoteUdpPort); //配置远端ip地址和端口
  Udp.print(replyPacket);                            //把数据写入发送缓冲区
  Udp.endPacket();                                   //发送数据
  Serial.println("UDP数据发送成功");
  int packetSize = Udp.parsePacket(); //获得解析包
  if (packetSize)                     //解析包不为空
  {
    //收到Udp数据包
    // Udp.remoteIP().toString().c_str()用于将获取的远端IP地址转化为字符串
    Serial.printf("收到来自远程IP：%s（远程端口：%d）的数据包字节数：%d\n", Udp.remoteIP().toString().c_str(), Udp.remotePort(), packetSize);

    // 读取Udp数据包并存放在incomingPacket
    int len = Udp.read(incomingPacket, 255); //返回数据包字节数
    if (len > 0)
    {
      incomingPacket[len] = 0;                                //清空缓存
      Serial.printf("UDP数据包内容为: %s\n", incomingPacket); //向串口打印信息
    }
  }
  delay(10000); //延时3秒
}
