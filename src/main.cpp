/*
引脚配置:
13 -> 显示屏时钟信号(scl)
14 -> 显示屏数据信号(sda)
25 -> 灯带pwm信号
*/
/*系统由freertos接管*/
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
// #include <BLEDevice.h>           //蓝牙依赖
// #include <BLEUtils.h>            //蓝牙依赖
// #include <BLEScan.h>             //蓝牙依赖
// #include <BLEAdvertisedDevice.h> //蓝牙依赖
#define BLINKER_PRINT Serial //Blinker.h依赖
#define BLINKER_WIFI         //Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#include "HTTPClient.h"
#include "Blinker.h"
#include "FastLED.h"
////////////////////////////////////////////////////////////////
//灯光初始化定义
#define NUM_LEDS 120
#define DATA_PIN 25
#define USE_MULTCORE 1
CRGB leds[NUM_LEDS];
TaskHandle_t rgb_run;
TaskHandle_t sitclock_run;
////////////////////////////////////////////////////////////////
//全局初始化
int8_t start_setup = 1;
struct tm timeinfo; //时间信息
int retry = 0;      //记录重试次数,全局变量
int ok = 0;
const char *ssid = u8"324-右"; //定义一个字符串(指针定义法)
const char *password = "21009200835";
//const char *ssid = "bszydxh"; //本地测试环境
//const char *password = "1357924680";
const char *auth = "AA7B63392ZQC";
const char *ntpServer = "cn.ntp.org.cn"; //时间服务器
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/13, /* data=*/14, /* reset=*/U8X8_PIN_NONE); //定义u8g2
////////////////////////////////////////////////////////////////
//blinker注册
BlinkerButton Button1("btn-abc");
BlinkerNumber Number1("num-abc");
BlinkerRGB RGB1("col-6ok");
////////////////////////////////////////////////////////////////
//灯光状态部分,字面意思
int8_t oled_mode = 1;      //显示屏模式 1 正常 0 关闭 2 欢迎 3 久坐
int8_t light_on = 0;       //灯带开关,用于状态回调,修改该值不能控制
int8_t mode = 0;           //!!!!灯光改变模式,并非小爱指定的模式!!!!
int8_t mi_mode = 0;        //小爱指定的模式,用于回调,与逻辑耦合的不是那么深,默认日光
int mi_light_bright = 100; //小爱指定的亮度,用于回调,与逻辑耦合的不是那么深,默认100
int8_t light_change = 0;
int light_now_brightness = 255;
int light_change_brightness = 255;
int light_brightness = 255;
int light_now_color_r = 0;
int light_now_color_g = 0;
int light_now_color_b = 0;
int32_t light_now = (255 * 256 + 150) * 256 + 50;
int light_change_color_r = 0xff;
int light_change_color_g = 0xff;
int light_change_color_b = 0xff;
int light_color_r = 255;
int light_color_g = 150;
int light_color_b = 50;
int8_t task2_running = 0;

////////////////////////////////////////////////////////////////
// 蓝牙部分//堆溢出,算了
// int scanTime = 5; //In seconds
// int scan_ok = 0;
// BLEScan *pBLEScan;
// class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
// {
//     void onResult(BLEAdvertisedDevice advertisedDevice)
//     {
//         //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
//         if (advertisedDevice.getAddress().toString() == "84:ab:26:a9:06:ce")
//         {
//             scan_ok = 1;
//             printf("Advertised Device:%d", advertisedDevice.getRSSI());
//         }
//         else
//         {
//             scan_ok = 0;
//             printf("can`t found device");
//         }
//     }
// };
// void scan()
// {
//     BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
//     Serial.println("Scan done!");
//     pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
// }
////////////////////////////////////////////////////////////////
//http请求部分,查天气,get
//
#define ARDUINOJSON_USE_LONG_LONG 1
#define URL "https://devapi.qweather.com/v7/weather/now?location=108.8325,34.1283&key=f890fb47ffff430b93bf22b085d03d07&gzip=n"
char text_final[30] = "   ";
char covid_final[30] = " ";
char temp_final[10] = " ";
char humidity_final[10] = " ";
char hitokoto_final[100] = "松花酿酒，春水煎茶。";
////////////////////////////////////////////////////////////////
//久坐提醒部分,默认1h
//算是手写看门狗...咬自己...
int blink_time = 5; //闪灯次数
int target_hour = -1;
int target_min = -1;
int sitclock_on = 0;  //状态指示
void reset_sitclock() //重置看门钟
{
    Serial.println("set_clock");
    if (timeinfo.tm_hour == 23)
    {
        target_hour = 0;
    }
    else
    {
        target_hour = timeinfo.tm_hour + 1;
    }
    target_min = timeinfo.tm_min;
    Serial.printf("set_clock:%d:%d\n", target_hour, target_min);
}
void reset_sitclock_limit()
{
    target_hour = timeinfo.tm_hour;
    target_min = timeinfo.tm_min + 10;
    if (target_min >= 60)
    {
        target_min = target_min - 60;
        target_hour++;
    }
    Serial.printf("set_clock_limit:%d:%d\n", target_hour, target_min);
}
//void sitclock_task(void *sitclock_task_pointer);
void off_sitclock() //跟关灯绑定
{
    if (sitclock_on != 0)
    {
        Serial.println("sitclock off!");
        sitclock_on = 0;
        vTaskDelete(sitclock_run);
    }
    target_hour = -1;
    target_min = -1;
}
int is_sitclock()
{
    if (sitclock_on == 1)
    {
        if (timeinfo.tm_hour == target_hour && timeinfo.tm_min == target_min)
        {
            return 1;
        }
    }
    return 0;
}
void sitclock_task(void *sitclock_task_pointer)
{
    while (1)
    {
        Serial.printf("sit clock try ");
        if (is_sitclock() == 1)
        {
            printf("sit clock warning!!!");
            //久坐之后的操作
            mode = 4;
            light_change = 1;
            oled_mode = 3;
            blink_time = 5;
            reset_sitclock_limit();
        }
        delay(5000);
    }
}
void on_sitclock() //跟开灯绑定(含类似行为)
{
    if (sitclock_on == 0)
    {
        reset_sitclock();
        xTaskCreatePinnedToCore(sitclock_task, "setclockTask", 2048, NULL, 0, &sitclock_run, 0);
    }
    sitclock_on = 1;
    Serial.println("sitclock start!");
}
////////////////////////////////////////////////////////////////
void esp32_Http_covid()
{
    //创建 HTTPClient 对象
    HTTPClient httpClient3;
    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient3.begin("https://lab.isaaclin.cn/nCoV/api/area?country=%E4%B8%AD%E5%9B%BD&province=%E9%99%95%E8%A5%BF%E7%9C%81&latest=true");
    httpClient3.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    Serial.print("URL: ");
    Serial.println("https://lab.isaaclin.cn/nCoV/api/area?country=%E4%B8%AD%E5%9B%BD&province=%E9%99%95%E8%A5%BF%E7%9C%81&latest=trueURL");
    httpClient3.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode3 = httpClient3.GET();
    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode3 == HTTP_CODE_OK)
    {
        //String responsePayload = httpClient3.getString();
        const String &payload3 = httpClient3.getString();
        Serial.println("Server Response Payload:");
        Serial.println(payload3);
        DynamicJsonDocument jsonBuffer3(4096);
        deserializeJson(jsonBuffer3, payload3);
        JsonObject root3 = jsonBuffer3.as<JsonObject>();
        //JsonArray now = root["now"];
        int covid = root3["results"][0]["cities"][0]["currentConfirmedCount"];

        if (covid != 0)
        {
            sprintf(covid_final, "%d", covid);
        }
        Serial.println(covid);
    }
    else
    {
        Serial.println("Server Respose Code:");
        Serial.println(httpCode3);
    }
    //关闭与服务器连接
    httpClient3.end();
}
void esp32_Http()
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
        const char *temp = root["now"]["temp"];
        const char *humidity = root["now"]["humidity"];
        if (text != NULL)
        {
            sprintf(text_final, "%s", text);
        }
        if (temp != NULL)
        {
            sprintf(temp_final, "%s", temp);
        }
        if (humidity != NULL)
        {
            sprintf(humidity_final, "%s", humidity);
        }
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
void esp32_Http_hitokoto()
{
    //创建 HTTPClient 对象
    HTTPClient httpClient2;

    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient2.begin("https://v1.hitokoto.cn/?encode=text&max_length=10");
    httpClient2.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    Serial.print("https://v1.hitokoto.cn/?encode=text&max_length=10");
    httpClient2.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode2 = httpClient2.GET();
    Serial.print("Send GET request to URL: ");

    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode2 == HTTP_CODE_OK)
    {
        //String responsePayload = httpClient.getString();
        const String &payload2 = httpClient2.getString();
        Serial.println("Server Response Payload:");
        Serial.println(payload2);
        if (payload2 != NULL)
        {
            sprintf(hitokoto_final, "%s", payload2.c_str());
        }
    }
    else
    {
        Serial.println("Server Respose Code:");
        Serial.println(httpCode2);
    }
    //关闭与服务器连接
    httpClient2.end();
}
//显示屏开关
void oled_show(const char *str1, const char *str2, const char *str3, const char *str4) //提供三行英文输出
{
    if (oled_mode == 1)
    {
        //char str_sum[100];//不需要日志注释掉
        //char *str = &str_sum[0];//不需要日志注释掉
        u8g2.clearBuffer();
        //u8g2.setFont(u8g2_font_ncenB12_tr);
        u8g2.setFont(u8g2_font_wqy16_t_gb2312);
        //u8g2.drawStr(0, 12, str1);
        //u8g2.drawStr(0, 27, str2);
        //u8g2.drawStr(0, 42, str3);
        // u8g2.drawStr(0, 57, str4);
        u8g2.setCursor(0, 13);
        u8g2.print(str1);
        u8g2.setCursor(0, 30);
        u8g2.print(str2);
        u8g2.setCursor(0, 47);
        u8g2.print(str3);
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(0, 62);
        u8g2.print(str4);
        //sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
        //Serial.println(str);//不需要日志注释掉
        Serial.println("oled_change");
        u8g2.sendBuffer();
    }
    else if (oled_mode == 0)
    {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        Serial.println("oled_off");
    }
    else if (oled_mode == 2)
    {
        //char str_sum[100];//不需要日志注释掉
        //char *str = &str_sum[0];//不需要日志注释掉
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_wqy16_t_gb2312);
        u8g2.setCursor(0, 13);
        u8g2.print("欢迎回来 bszydxh");
        u8g2.setCursor(0, 30);
        u8g2.print("灯带就绪...");
        u8g2.setCursor(0, 47);
        u8g2.print("显示屏就绪...");
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(0, 62);
        u8g2.print(str4);
        //sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
        //Serial.println(str);//不需要日志注释掉
        Serial.println("oled_change");
        u8g2.sendBuffer();
        delay(700);
        oled_mode = 1; //欢迎完正常
    }
    else if (oled_mode == 3)
    {
        //char str_sum[100];//不需要日志注释掉
        //char *str = &str_sum[0];//不需要日志注释掉
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_wqy16_t_gb2312);
        u8g2.setCursor(0, 13);
        u8g2.print("!!!久坐提醒!!!");
        u8g2.setCursor(0, 30);
        u8g2.print("!!!起来动动!!!");
        u8g2.setCursor(0, 47);
        u8g2.print("!!!看看远方!!!");
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(0, 62);
        u8g2.print(str4);
        //sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
        //Serial.println(str);//不需要日志注释掉
        Serial.println("oled_change");
        u8g2.sendBuffer();
        delay(10000);
        oled_mode = 1; //提示完正常
    }
}
void print_time() //常驻显示任务,必须循环,否则出事
{

    if (!getLocalTime(&timeinfo)) //获取时间不成功(一次也没)...允悲
    {
        //oled_show("error:404", "pls wait", "retrying...", "");
        Serial.println("error:no connect");
        char retry_str[50] = "0";
        sprintf(retry_str, "重连次数: %d", retry);
        if (retry % 3 == 1)
        {
            oled_show("smart_screen", "---bszydxh", "无网络.", retry_str);
        }
        if (retry % 3 == 2)
        {
            oled_show("smart_screen", "---bszydxh", "无网络..", retry_str);
        }
        if (retry % 3 == 0)
        {
            oled_show("smart_screen", "---bszydxh", "无网络...", retry_str);
        }
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        retry++;
        if (retry == 20)
        {
            Serial.println("error:no connect");
            oled_show("smart_screen", "系统错误", "请等待", "正在重启...");
            esp_restart();
        }
        return;
    }
    char str1[60];
    char str2[60];
    char str3[60];
    char st[60];
    char strrr[60];
    strftime(st, 100, "%a", &timeinfo);
    sprintf(str1, "%4d-%02d-%02d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, st); //整合字符串
    strftime(strrr, 100, "%H:%M:%S", &timeinfo);
    if (task2_running == 0)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            sprintf(str2, "%s 在线", strrr);
        }
        else
        {
            sprintf(str2, "%s 离线", strrr);
        }
    }
    else
    {
        sprintf(str2, "%s USB", strrr);
    }
    //sprintf(str2, "%s %s", strrr, text_final);
    if (timeinfo.tm_sec % 10 >= 5)
    {
        sprintf(str3, "%s|%s℃ %s", text_final, temp_final, covid_final);
    }
    else
    {
        sprintf(str3, "%s|%s%% 西安", text_final, humidity_final);
    }

    oled_show(str1, str2, str3, hitokoto_final);
    /*if (ok == 0)
    w{
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
    Serial.print("Ada\n");
    rgb_screen_on = 1;
    light_change = 1;
}
void rgb1_callback(uint8_t r_value, uint8_t g_value, uint8_t b_value, uint8_t bright_value)
{
    light_on = 1; //强制开启
    light_now = (r_value * 256 + g_value) * 256 + b_value;
    Serial.println("color change");
    light_color_r = r_value;
    light_color_g = g_value;
    light_color_b = b_value;
    Serial.print("blinker_color||r:");
    Serial.print(light_color_r);
    Serial.print("  g:");
    Serial.print(light_color_g);
    Serial.print("  b:");
    Serial.println(light_color_b);
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
        oled_mode = 2;
        Serial.println("light on");
        light_change = 1;
        mode = 1;
        on_sitclock();
        //oled_show("", "", "light on");
        BlinkerMIOT.powerState("on");
        BlinkerMIOT.print();
    }
    else if (state == BLINKER_CMD_OFF)
    {
        //oled_show("", "", "light off");
        Serial.println("light off");
        off_sitclock();
        light_change = 1;
        mode = 0;
        oled_mode = 0;
        light_on = 0;

        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
}
void miotColor(int32_t color)
{
    BLINKER_LOG("need set color: ", color);
    light_on = 1; //强制开启
    light_now = color;
    uint8_t colorR = color >> 16 & 0xFF;
    uint8_t colorG = color >> 8 & 0xFF;
    uint8_t colorB = color & 0xFF;
    Serial.printf("set color: %d\n", color);
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
    on_sitclock();
    light_change = 1;
    mode = 3;
    BlinkerMIOT.color(color);
    BlinkerMIOT.print();
    //oled_show("", "color", "change");
}
void miotBright(const String &bright)
{
    light_on = 1; //强制开启
    mi_light_bright = bright.toInt();
    BLINKER_LOG("need set brightness: ", bright);
    light_brightness = bright.toInt() / 2 + bright.toInt() * 2;
    mode = 3;
    light_change = 1;
    on_sitclock();
    BLINKER_LOG("now set brightness: ", light_brightness);
    BlinkerMIOT.brightness(light_brightness);
    BlinkerMIOT.print();
    //oled_show("", "brightness", "change");
}
void miotMode(uint8_t mode_mi)
{
    BLINKER_LOG("need set mode: ", mode_mi);
    mi_mode = mode_mi;
    if (mode_mi == BLINKER_CMD_MIOT_DAY)
    {
        if (task2_running == 1)
        {
            vTaskDelete(rgb_run);
            task2_running = 0;
        }
        on_sitclock();
        light_color_r = 255;
        light_color_g = 150;
        light_color_b = 50;
        Serial.print("mi_color||r:");
        Serial.print(light_color_r);
        Serial.print("  g:");
        Serial.print(light_color_g);
        Serial.print("  b:");
        Serial.println(light_color_b);
        light_brightness = 255;
        light_change = 1;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_NIGHT)
    {
        if (task2_running == 1)
        {
            vTaskDelete(rgb_run);
            task2_running = 0;
        }
        on_sitclock();
        light_color_r = 0;
        light_color_g = 0;
        light_color_b = 255;
        Serial.print("mi_color||r:");
        Serial.print(light_color_r);
        Serial.print("  g:");
        Serial.print(light_color_g);
        Serial.print("  b:");
        Serial.println(light_color_b);
        light_brightness = 120;
        light_change = 1;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COLOR)
    {
        // Your mode function
    }
    else if (mode_mi == BLINKER_CMD_MIOT_WARMTH)
    {
        // Your mode function
    }
    else if (mode_mi == BLINKER_CMD_MIOT_TV)
    {
        // Your mode function
    }
    else if (mode_mi == BLINKER_CMD_MIOT_READING)
    {
        if (task2_running == 1)
        {
            vTaskDelete(rgb_run);
            task2_running = 0;
        }
        on_sitclock();
        light_color_r = 255;
        light_color_g = 150;
        light_color_b = 50;
        Serial.print("mi_color||r:");
        Serial.print(light_color_r);
        Serial.print("  g:");
        Serial.print(light_color_g);
        Serial.print("  b:");
        Serial.println(light_color_b);
        light_change = 1;
        light_brightness = 255;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COMPUTER)
    {
        Serial.print("Ada\n");
        rgb_screen_on = 1;
        light_change = 1;
    }
    BlinkerMIOT.mode(mode_mi);
    BlinkerMIOT.print();
}
void miotQuery(int32_t queryCode)
{
    BLINKER_LOG("MIOT Query codes: ", queryCode);

    switch (queryCode)
    {
    case BLINKER_CMD_QUERY_ALL_NUMBER:
        BLINKER_LOG("MIOT Query All");
        BlinkerMIOT.powerState(light_on ? "on" : "off");
        BlinkerMIOT.color(light_now);
        BlinkerMIOT.mode(mi_mode);
        //BlinkerMIOT.colorTemp(1000);
        BlinkerMIOT.brightness(mi_light_bright);
        BlinkerMIOT.print();
        break;
    case BLINKER_CMD_QUERY_POWERSTATE_NUMBER:
        BLINKER_LOG("MIOT Query Power State");
        BlinkerMIOT.powerState(light_on ? "on" : "off");
        BlinkerMIOT.print();
        break;
    case BLINKER_CMD_QUERY_COLOR_NUMBER:
        BLINKER_LOG("MIOT Query Color");
        BlinkerMIOT.color(light_now);
        BlinkerMIOT.print();
        break;
    case BLINKER_CMD_QUERY_MODE_NUMBER:
        BLINKER_LOG("MIOT Query Mode");
        BlinkerMIOT.mode(mi_mode);
        BlinkerMIOT.print();
        break;
    case BLINKER_CMD_QUERY_COLORTEMP_NUMBER:
        BLINKER_LOG("MIOT Query ColorTemperature");
        //BlinkerMIOT.colorTemp(1000);
        BlinkerMIOT.print();
        break;
    case BLINKER_CMD_QUERY_BRIGHTNESS_NUMBER:
        BLINKER_LOG("MIOT Query Brightness");
        BlinkerMIOT.brightness(mi_light_bright);
        BlinkerMIOT.print();
        break;
    default:
        BlinkerMIOT.powerState(light_on ? "on" : "off");
        BlinkerMIOT.color(light_now);
        BlinkerMIOT.mode(mi_mode);
        //BlinkerMIOT.colorTemp(1000);
        BlinkerMIOT.brightness(mi_light_bright);
        BlinkerMIOT.print();
        break;
    }
}
void xTaskOne(void *xTask1) //显示屏任务
{
    while (1)
    {
        print_time();
        delay(300);
    }
}

void xTaskTwo(void *xTask2) //流光溢彩任务
{
    while (1)
    {
        rgb_screen();
    }
}
void xTaskThree(void *xTask3) //blinker任务
{
    while (1)
    {
        Blinker.run();
        delay(10);
    }
}
void xTaskFour(void *xTask4) //巨型http请求模块任务
{
    while (1)
    {
        delay(200);
        if (start_setup == 1)
        {
            delay(10000);
            esp32_Http();
            esp32_Http_hitokoto();
            esp32_Http_covid();
            start_setup = 0;
        }
        if (timeinfo.tm_min % 3 == 0 && timeinfo.tm_sec == 0)
        {
            esp32_Http();
        }
        if (timeinfo.tm_sec == 45)
        {
            esp32_Http_hitokoto();
            esp32_Http_covid();
        }
    }
}
// void xTaskFive(void *xTask5) //蓝牙任务
// {
//         BLEDevice::init("esp32");
//         vTaskDelete(NULL);
// }
void light()
{
    if (light_change == 1 && task2_running == 0) //1 全色
    {
        // Move a single white led
        if (rgb_screen_on == 1)
        {
            rgb_screen_on = 0;
            task2_running = 1;
            Serial.println("light");
            off_sitclock();
#if !USE_MULTCORE
            xTaskCreate(xTaskTwo, "TaskTwo", 4096, NULL, 2, NULL);
#else
            xTaskCreatePinnedToCore(xTaskTwo, "TaskOne", 4096, NULL, -1, &rgb_run, 1);
#endif

            return;
        }
        if (mode == 1 || mode == 3) //换亮度/色彩
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
        else if (mode == 4) //久坐闪灯
        {
            if (blink_time > 0)
            {
                for (int8_t n = 1; n <= 24; n++)
                {
                    light_change_color_r = light_now_color_r + (255 - light_now_color_r) * n / 24;
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
                Serial.println("led blink");
                delay(1000);
                for (int8_t n = 1; n <= 24; n++)
                {
                    light_change_color_r = 255 + (light_change_color_r - 255) * n / 24;
                    light_change_color_g = 0 + (light_now_color_g)*n / 24;
                    light_change_color_b = 0 + (light_now_color_b)*n / 24;
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
                delay(1000);
                Serial.println("led blink");
                blink_time--;
            }
            else
            {
                light_change = 0;
                Serial.println("blink failed");
            }
        }
    }
}
void xTaskSix(void *xTask6) //灯条任务
{
    while (1)
    {
        light();
        delay(100);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("bszydxh esp32 start!");
    WiFi.mode(WIFI_STA);
    //BLEDevice::init("esp32");
    // pBLEScan = BLEDevice::getScan(); //create new scan
    // pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    // pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
    // pBLEScan->setInterval(1000);
    // pBLEScan->setWindow(99); // less or equal setInterval value
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

#if defined(BLINKER_PRINT)
    BLINKER_DEBUG.stream(BLINKER_PRINT);
#endif
    u8g2.begin();
    u8g2.enableUTF8Print();
    WiFi.begin(ssid, password);

    oled_show("smart_screen", "---bszydxh", "搜索wifi中...", "初始化灯带...");
    while (WiFi.status() != WL_CONNECTED) //线程阻断,等待网络连接
    {
        delay(1000);
        retry++;
        Serial.println("no wifi!");
        Serial.println(ssid);
        char retry_str[50] = "0";
        sprintf(retry_str, "扫描次数: %d", retry);
        if (retry % 3 == 1)
        {
            oled_show("smart_screen", "---bszydxh", "wifi扫描中.", retry_str);
        }
        if (retry % 3 == 2)
        {
            oled_show("smart_screen", "---bszydxh", "wifi扫描中..", retry_str);
        }
        if (retry % 3 == 0)
        {
            oled_show("smart_screen", "---bszydxh", "wifi扫描中...", retry_str);
        }
        if (retry == 15)
        {
            Serial.println("error:no wifi");
            oled_show("smart_screen", "系统错误", "请等待", "正在重启...");
            esp_restart();
        }
    }
    retry = 0;
    oled_show("smart_screen", "---bszydxh", "连接成功", "加载系统中...");
    Serial.println("wifi! done");
    Blinker.begin(auth, ssid, password);
    Blinker.attachData(dataRead);
    Button1.attach(button1_callback);
    RGB1.attach(rgb1_callback);
    BlinkerMIOT.attachPowerState(miotPowerState);
    BlinkerMIOT.attachColor(miotColor);
    BlinkerMIOT.attachMode(miotMode);
    BlinkerMIOT.attachBrightness(miotBright);
    BlinkerMIOT.attachQuery(miotQuery);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    light_change = 1;
    xTaskCreatePinnedToCore(xTaskOne, "TaskOne", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(xTaskThree, "TaskThree", 7168, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(xTaskFour, "TaskFour", 5120, NULL, 0, NULL, 0);
    xTaskCreatePinnedToCore(xTaskSix, "TaskSix", 1024, NULL, -1, NULL, 1);
}
void loop()
{
    //Blinker.run(); //wifi blinker自动处理 不用管
    //light();
    //Serial.printf("Freeheap:%d\n", xPortGetFreeHeapSize());
    //Serial.printf("FreeMinheap:%d\n", xPortGetMinimumEverFreeHeapSize());
    delay(500); //踢看门狗,lopp本质上也是freertos中的一个任务
}