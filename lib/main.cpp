#include <Arduino.h> //主依赖,具体依赖见依赖树
#include "SPI.h"     //U8g2.h依赖 Blinker.h依赖
#include "Wire.h"    //U8g2.h依赖
#include "U8g2lib.h"
#include "WiFi.h"             //Blinker.h依赖
#include "ESPmDNS.h"          //Blinker.h依赖
#include "FS.h"               //Blinker.h依赖
#include "SPIFFS.h"           //Blinker.h依赖
#include "Ticker.h"           //Blinker.h依赖
#include "Update.h"           //Blinker.h依赖
#include "WiFiClientSecure.h" //Blinker.h依赖
#include "EEPROM.h"           //Blinker.h依赖
#define BLINKEER_PRINT Serial //Blinker.h依赖
#define BLINKER_WIFI          //Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#include "Blinker.h"
#include "FastLED.h"
/**********************************************************************
  项目名称/Project          : 零基础入门学用物联网
  程序名称/Program name     : HTTPClient_begin
  团队/Team                : 太极创客团队 / Taichi-Maker (www.taichi-maker.com)
  作者/Author              : Dapenson
  日期/Date（YYYYMMDD）     : 20200325
  程序目的/Purpose          :
  此程序用于演示如何使用ESP8266来向www.example.com网站服务器发送GET请求并通过串口
  监视器将网站服务器响应信息通过串口监视器显示出来。
  -----------------------------------------------------------------------
  修订历史/Revision History  
  日期/Date    作者/Author      参考号/Ref    修订说明/Revision Description
  20200411      CYNO朔           001        将请求服务器改为www.example.com
  -----------------------------------------------------------------------
  本示例程序为太极创客团队制作的《零基础入门学用物联网》中示例程序。
  该教程为对物联网开发感兴趣的朋友所设计和制作。如需了解更多该教程的信息，请参考以下网页：
  http://www.taichi-maker.com/homepage/esp8266-nodemcu-iot/iot-c/esp8266-nodemcu-web-client/http-request/
***********************************************************************/
#include <HTTPClient.h>
//#include "ArduinoJson.h"

// 测试HTTP请求用的URL

#define URL "https://devapi.qweather.com/v7/weather/now?location=108.8325,34.1283&key=f890fb47ffff430b93bf22b085d03d07&gzip=n"
//#define URL "https://nmsl.shadiao.app/api.php?from=xxx"
//#define URL "https://oapi.dingtalk.com/robot/send?access_token=9db4d9fd558fd1bd70d8a5f1e7a79c6a185ff4529724e13f7ad50d56bd10bb45"
// 设置wifi接入信息(请根据您的WiFi信息进行修改)
const char *ssid = "bszydxh";
const char *password = "1357924680";
// 发送HTTP请求并且将服务器响应通过串口输出
void esp8266Http()
{

    //创建 HTTPClient 对象
    HTTPClient httpClient;

    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient.begin(URL);
    httpClient.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    Serial.print("URL: ");
    Serial.println(URL);
    httpClient.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode = httpClient.GET();
    Serial.print("Send GET request to URL: ");
    Serial.println(URL);

    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode == HTTP_CODE_OK)
    {
        //String responsePayload = httpClient.getString();
        const String &payload = httpClient.getString();
        Serial.println("Server Response Payload:");
        Serial.println(payload);
        DynamicJsonDocument jsonBuffer(2048);
        deserializeJson(jsonBuffer, payload);
        JsonObject root = jsonBuffer.as<JsonObject>();
        //JsonArray now = root["now"];
        const char *text = root["now"]["text"];
        Serial.println(text);
    }
    else
    {
        Serial.println("Server Respose Code:");
        Serial.println(httpCode);
    }

    //关闭与服务器连接
    httpClient.end();
}
void setup()
{
    //初始化串口设置
    Serial.begin(115200);

    //设置ESP8266工作模式为无线终端模式
    WiFi.mode(WIFI_STA);

    //开始连接wifi
    WiFi.begin(ssid, password);

    //等待WiFi连接,连接成功打印IP
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("WiFi Connected!");
}

void loop()
{
    // 如果ESP8266连接WiFi则发送HTTP请求
    if ((WiFi.status() == WL_CONNECTED))
    {
        esp8266Http();
    }

    delay(5000); // 短暂等待
}
