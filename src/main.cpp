#include <Arduino.h>
#include "SPI.h"
#include "Wire.h"
#include "U8g2lib.h"
#include "WiFi.h"
int ok = 0;
const char *ssid = "bszydxh";
const char *password = "1357924680";
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/13, /* data=*/14, /* reset=*/U8X8_PIN_NONE);
void print_time()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr(0, 20, "error:404");
        u8g2.drawStr(0, 40, "pls wait");
        u8g2.drawStr(0, 60, "retrying...");
        Serial.println("error:no time");
        u8g2.sendBuffer();
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        return;
    }
    char str1[30];
    char str2[30];
    char str3[30];
    sprintf(str1, "%4d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    strftime(str2,100,"%H:%M:%S",&timeinfo);
    strftime(str3,100,"%A",&timeinfo);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 20, str1);
    u8g2.drawStr(0, 40, str2);
    u8g2.drawStr(0, 60, str3);
    u8g2.sendBuffer();
    if (ok == 0)
    {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        ok = 1;
    }

    Serial.println(&timeinfo, "%F %T %A");
}
void setup()
{
    Serial.begin(115200);
    Serial.println();
    WiFi.begin(ssid, password);
    u8g2.begin();
    while (WiFi.status() != WL_CONNECTED)
    {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr(0, 20, "esp32");
        u8g2.drawStr(0, 40, "---bszydxh");
        u8g2.drawStr(0, 60, "no wifi...");
        u8g2.sendBuffer();
        Serial.println("error:no wifi");
        delay(500);
    }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 20, "esp32");
    u8g2.drawStr(0, 40, "---bszydxh");
    u8g2.drawStr(0, 60, "wifi ok...");
    u8g2.sendBuffer();
    Serial.println("wifi done");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(500);
    print_time();
}

void loop()
{
    delay(100);
    print_time();
}