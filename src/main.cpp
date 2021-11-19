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
int ok = 0;
const char *ssid = "bszydxh"; //定义一个字符串(指针定义法)
const char *password = "1357924680";
const char *auth = "AA7B63392ZQC";
const char *ntpServer = "pool.ntp.org"; //时间服务器
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/13, /* data=*/14, /* reset=*/U8X8_PIN_NONE);
BlinkerButton Button1("btn-abc");
BlinkerNumber Number1("num-abc");
int light_on = 1;
void oled_show(const char *str1, const char *str2, const char *str3) //提供三行英文输出
{
    if (light_on == 1)
    {
        //char str_sum[100];//不需要日志注释掉
        //char *str = &str_sum[0];//不需要日志注释掉
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr(0, 20, str1);
        u8g2.drawStr(0, 40, str2);
        u8g2.drawStr(0, 60, str3);
        //sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
        //Serial.println(str);//不需要日志注释掉
        Serial.println("oled_change");
        u8g2.sendBuffer();
    }else if(light_on == 0){
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        Serial.println("oled_off");

    }
}
void print_time()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        oled_show("error:404", "pls wait", "retrying...");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        return;
    }
    char str1[30];
    char str2[30];
    char str3[30];
    sprintf(str1, "%4d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    strftime(str2, 100, "%H:%M:%S", &timeinfo);
    if (WiFi.status() == WL_CONNECTED)
    {
        strftime(str3, 100, "%a ONL", &timeinfo);
    }
    else
    {
        strftime(str3, 100, "%a OFL", &timeinfo);
    }

    oled_show(str1, str2, str3);
    /*if (ok == 0)
    {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        ok = 1;
    }*/
    delay(100);
    //Serial.println(&timeinfo, "%F %T %A");//日志
}
int counter = 0;
void button1_callback(const String &state)
{
    BLINKER_LOG("get button state:", state);
    Serial.println("button done");
    oled_show("", "", "button on");
}
void dataRead(const String &data)
{
    BLINKER_LOG("Blinker readString: ", data);
    counter++;
    Number1.print(counter);
}
void miotPowerState(const String &state)
{
    BLINKER_LOG("need set power state: ", state);

    if (state == BLINKER_CMD_ON)
    {
        Serial.println("light on");
        light_on = 1;
        oled_show("", "", "light on");
        BlinkerMIOT.powerState("on");
        BlinkerMIOT.print();
    }
    else if (state == BLINKER_CMD_OFF)
    {
        Serial.println("light off");
        light_on = 0;
        oled_show("", "", "light off");
        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
}
void setup()
{
    Serial.begin(115200);
    Serial.println("bszydxh esp32 start!");
#if defined(BLINKER_PRINT)
    BLINKER_DEBUG.stream(BLINKER_PRINT)
#endif
        u8g2.begin();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) //线程阻断,等待网络连接
    {
        Serial.println("no wifi!");
        oled_show("esp32", "---bszydxh", "no wifi...");
        delay(1000);
    }
    oled_show("esp32", "---bszydxh", "wifi ok...");
    Serial.println("wifi! done");
    Blinker.begin(auth, ssid, password);
    Blinker.attachData(dataRead);
    Button1.attach(button1_callback);
    BlinkerMIOT.attachPowerState(miotPowerState);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(500);
    print_time();
}

void loop()
{
    Blinker.run();
    print_time();
}