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
#define BLINKER_PRINT Serial  //Blinker.h依赖
#define BLINKER_WIFI          //Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#include "Blinker.h"
#include "FastLED.h"
#define NUM_LEDS 120
#define DATA_PIN 25
#define USE_MULTCORE 0
CRGB leds[NUM_LEDS];
int retry = 0; //记录重试次数,全局变量
int ok = 0;
const char *ssid = u8"324-右"; //定义一个字符串(指针定义法)
const char *password = "21009200835";
//const char *ssid = "bszydxh"; //本地测试环境
//const char *password = "1357924680";
const char *auth = "AA7B63392ZQC";
const char *ntpServer = "pool.ntp.org"; //时间服务器
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/13, /* data=*/14, /* reset=*/U8X8_PIN_NONE); //定义u8g2
BlinkerButton Button1("btn-abc");
BlinkerNumber Number1("num-abc");
BlinkerRGB RGB1("col-6ok");
int8_t light_on = 1;
int8_t mode = 0;
int8_t light_change = 0;
int light_now_brightness = 255;
int light_change_brightness = 255;
int light_brightness = 255;
int light_now_color_r = 0;
int light_now_color_g = 0;
int light_now_color_b = 0;
int light_change_color_r = 0xff;
int light_change_color_g = 0xff;
int light_change_color_b = 0xff;
int light_color_r = 255;
int light_color_g = 150;
int light_color_b = 50;

//显示屏开关
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
    }
    else if (light_on == 0)
    {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        Serial.println("oled_off");
    }
}
void print_time() //常驻显示任务,必须循环,否则出事
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) //获取时间不成功(一次也没)...允悲
    {
        oled_show("error:404", "pls wait", "retrying...");
        Serial.println("error:no connect");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        retry++;
        if (retry == 20)
        {
            Serial.println("error:no connect");
            oled_show("error:sys", "pls wait", "restarting...");
            esp_restart();
        }
        return;
    }
    char str1[30];
    char str2[30];
    char str3[30];
    sprintf(str1, "%4d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday); //整合字符串
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
    //Serial.println(&timeinfo, "%F %T %A");//日志
}
int counter = 0; //官方计数
int rgb_screen_on = 0;
void button1_callback(const String &state)
{
    BLINKER_LOG("get button state:", state);
    Serial.print("Ada\n");
    rgb_screen_on = 1;
    light_change = 1;
    Serial.println("button done");
    //oled_show("", "", "button on");
}
void rgb1_callback(uint8_t r_value, uint8_t g_value, uint8_t b_value, uint8_t bright_value)
{
    BLINKER_LOG("R value: ", r_value);
    BLINKER_LOG("G value: ", g_value);
    BLINKER_LOG("B value: ", b_value);
    BLINKER_LOG("Rrightness value: ", bright_value);
    light_brightness = bright_value;
    light_color_r = (int)r_value;
    light_color_g = (int)g_value;
    light_color_b = (int)b_value;
    Serial.println("rgb change");
    light_change = 1;
    mode = 3;
}
void dataRead(const String &data)
{
    BLINKER_LOG("Blinker readString: ", data);
    counter++;
    Number1.print(counter);
}
uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;
void rgb_screen()
{
    //Serial.println("rgb_show");
    // Wait for first byte of Magic Word
    //魔法包结构 Ada+校验码+rgb数据
    for (i = 0; i < sizeof prefix; ++i) //读到Ada开始
    {
    waitLoop:
        while (!Serial.available())
            ;
        ;
        // Check next byte in Magic Word
        if (prefix[i] == Serial.read())
            continue;
        // otherwise, start over
        i = 0;
        goto waitLoop;
    }

    // Hi, Lo, Checksum
    while (!Serial.available())
        ;
    ;
    hi = Serial.read();
    while (!Serial.available())
        ;
    ;
    lo = Serial.read();
    while (!Serial.available())
        ;
    ;
    chk = Serial.read();
    //检查校验码
    if (chk != (hi ^ lo ^ 0x55))
    {
        i = 0;
        goto waitLoop;
    }                                                //线程阻断
    memset(leds, 0, NUM_LEDS * sizeof(struct CRGB)); //将leds空间置零
    // Read the transmission data and set LED values
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        byte r, g, b;
        while (!Serial.available()) //读取阻断
            ;
        r = Serial.read();
        while (!Serial.available()) //读取阻断
            ;
        g = Serial.read();
        while (!Serial.available()) //读取阻断
            ;
        b = Serial.read();
        leds[i].r = r;
        leds[i].g = g;
        leds[i].b = b;
    }

    // Shows new values
    FastLED.show();
}
void miotPowerState(const String &state)
{
    BLINKER_LOG("need set power state: ", state);

    if (state == BLINKER_CMD_ON)
    {
        light_on = 1;
        Serial.println("light on");
        light_change = 1;
        mode = 1;
        //oled_show("", "", "light on");
        BlinkerMIOT.powerState("on");
        BlinkerMIOT.print();
    }
    else if (state == BLINKER_CMD_OFF)
    {
        //oled_show("", "", "light off");
        Serial.println("light off");
        light_change = 1;
        mode = 0;
        light_on = 0;
        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
}
void miotColor(int32_t color)
{
    BLINKER_LOG("need set color: ", color);

    int32_t colorR = color >> 16 & 0xFF;
    int32_t colorG = color >> 8 & 0xFF;
    int32_t colorB = color & 0xFF;
    Serial.println("color change");
    BLINKER_LOG("colorR: ", colorR, ", colorG: ", colorG, ", colorB: ", colorB);
    light_color_r = colorR;
    light_color_g = colorG;
    light_color_b = colorB;
    if (light_color_r == 255 && light_color_g == 255 && light_color_b == 0)
    {
        light_color_r = 255;
        light_color_g = 150;
        light_color_b = 50;
    }
    Serial.print("mi_color||r:");
    Serial.print(light_color_r);
    Serial.print("  g:");
    Serial.print(light_color_g);
    Serial.print("  b:");
    Serial.println(light_color_b);
    light_change = 1;
    mode = 3;
    BlinkerMIOT.color(color);
    BlinkerMIOT.print();
    //oled_show("", "color", "change");
}
void miotBright(const String &bright)
{
    BLINKER_LOG("need set brightness: ", bright);
    light_brightness = bright.toInt() / 2 + bright.toInt() * 2;
    mode = 3;
    light_change = 1;
    BLINKER_LOG("now set brightness: ", light_brightness);
    BlinkerMIOT.brightness(light_brightness);
    BlinkerMIOT.print();
    //oled_show("", "brightness", "change");
}
void xTaskOne(void *xTask1)
{
    while (1)
    {
        print_time();
        delay(100);
    }
}
void xTaskTwo(void *xTask2)
{
    while (1)
    {
        rgb_screen();
    }
}
void light()
{
    if (light_change == 1) //1 全色
    {
        // Move a single white led
        if (rgb_screen_on == 1)
        {
#if !USE_MULTCORE
            xTaskCreate(xTaskTwo, "TaskTwo", 4096, NULL, 2, NULL);
#else
            xTaskCreatePinnedToCore(xTaskTwo, "TaskOne", 4096, NULL, 2, NULL, 0);
#endif
            rgb_screen_on = 0;
            return;
        }
        if (mode == 1 || mode == 3)
        {
            for (int8_t n = 1; n <= 24; n++)
            {
                light_change_color_r = light_now_color_r + (light_color_r - light_now_color_r) * n / 24;
                light_change_color_g = light_now_color_g + (light_color_g - light_now_color_g) * n / 24;
                light_change_color_b = light_now_color_b + (light_color_b - light_now_color_b) * n / 24;
                light_change_brightness = light_now_brightness + (light_brightness - light_now_brightness) * n / 24;
                for (int8_t i = 0; i < n; i++)
                {

                    leds[23 - i].r = light_change_color_r;
                    leds[23 - i].g = light_change_color_g;
                    leds[23 - i].b = light_change_color_b;
                    leds[24 + i].r = light_change_color_r;
                    leds[24 + i].g = light_change_color_g;
                    leds[24 + i].b = light_change_color_b;
                    leds[71 - i].r = light_change_color_r;
                    leds[71 - i].g = light_change_color_g;
                    leds[71 - i].b = light_change_color_b;
                    leds[72 + i].r = light_change_color_r;
                    leds[72 + i].g = light_change_color_g;
                    leds[72 + i].b = light_change_color_b;
                    leds[96 + i].r = light_change_color_r;
                    leds[96 + i].g = light_change_color_g;
                    leds[96 + i].b = light_change_color_b;
                }
                FastLED.setBrightness(light_change_brightness);
                light_change = 0;
                Serial.print("r:");
                Serial.print(light_change_color_r);
                Serial.print("  g:");
                Serial.print(light_change_color_g);
                Serial.print("  b:");
                Serial.print(light_change_color_b);
                Serial.print("  l:");
                Serial.println(light_change_brightness);
                delay(10);
                FastLED.show();
            }
            light_now_color_r = light_color_r;
            light_now_color_g = light_color_g;
            light_now_color_b = light_color_b;
            light_now_brightness = light_brightness;
            Serial.println("led on");
        }
        else if (mode == 0) //关led
        {

            for (int8_t n = 1; n <= 24; n++)
            {
                light_change_color_r = light_now_color_r + (0 - light_now_color_r) * n / 24;
                light_change_color_g = light_now_color_g + (0 - light_now_color_g) * n / 24;
                light_change_color_b = light_now_color_b + (0 - light_now_color_b) * n / 24;
                for (int8_t i = 0; i < n; i++)
                {
                    leds[23 - i].r = light_change_color_r;
                    leds[23 - i].g = light_change_color_g;
                    leds[23 - i].b = light_change_color_b;
                    leds[24 + i].r = light_change_color_r;
                    leds[24 + i].g = light_change_color_g;
                    leds[24 + i].b = light_change_color_b;
                    leds[71 - i].r = light_change_color_r;
                    leds[71 - i].g = light_change_color_g;
                    leds[71 - i].b = light_change_color_b;
                    leds[72 + i].r = light_change_color_r;
                    leds[72 + i].g = light_change_color_g;
                    leds[72 + i].b = light_change_color_b;
                    leds[96 + i].r = light_change_color_r;
                    leds[96 + i].g = light_change_color_g;
                    leds[96 + i].b = light_change_color_b;
                }
                light_change = 0;
                Serial.print("r:");
                Serial.print(light_change_color_r);
                Serial.print("  g:");
                Serial.print(light_change_color_g);
                Serial.print("  b:");
                Serial.print(light_change_color_b);
                Serial.print("  l:");
                Serial.println(light_change_brightness);
                delay(10);
                FastLED.show();
            }
            Serial.println("led off");
            light_now_color_r = 0;
            light_now_color_g = 0;
            light_now_color_b = 0;
        }
    }
}
void setup()
{
    Serial.begin(115200);
    Serial.println("bszydxh esp32 start!");
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
#if defined(BLINKER_PRINT)
    BLINKER_DEBUG.stream(BLINKER_PRINT);
#endif
    u8g2.begin();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) //线程阻断,等待网络连接
    {
        retry++;
        Serial.println("no wifi!");
        Serial.println(ssid);
        oled_show("esp32", "---bszydxh", "no wifi...");
        if (retry == 15)
        {
            Serial.println("error:no wifi");
            oled_show("error:sys", "pls wait", "restarting...");
            esp_restart();
        }
        delay(1000);
    }
    retry = 0;
    oled_show("esp32", "---bszydxh", "wifi ok...");
    Serial.println("wifi! done");
    Blinker.begin(auth, ssid, password);
    Blinker.attachData(dataRead);
    Button1.attach(button1_callback);
    RGB1.attach(rgb1_callback);
    BlinkerMIOT.attachPowerState(miotPowerState);
    BlinkerMIOT.attachColor(miotColor);
    BlinkerMIOT.attachBrightness(miotBright);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    light_change = 1;
#if !USE_MULTCORE
    xTaskCreate(xTaskOne, "TaskOne", 4096, NULL, 1, NULL);
    //xTaskCreate(xTaskTwo, "TaskTwo", 4096, NULL, 2, NULL);
#else
    xTaskCreatePinnedToCore(xTaskOne, "TaskOne", 4096, NULL, 1, NULL, 1);
    //xTaskCreatePinnedToCore(xTaskTwo, "TaskOne", 4096, NULL, 2, NULL, 1);
#endif
}
void loop()
{
    Blinker.run(); //wifi blinker自动处理 不用管
    light();
}