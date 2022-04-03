/*
引脚配置:
13 -> 显示屏时钟信号(scl)
14 -> 显示屏数据信号(sda)
25 -> 灯带pwm信号
*/
/*
EEPROM 3000-5120自定义
3000 r1
3004 g1
3008 b1
3012 r2
3016 g2
3020 b2
3024 r3
3028 g3
3032 b3
*/
/*系统由freertos接管*/
/*delay()就是vTaskdelay(),不信自己跳转看一下*/
#include <Arduino.h> //主依赖,具体依赖见依赖树
#include "SPI.h"     //U8g2.h依赖 Blinker.h依赖
#include "Wire.h"    //U8g2.h依赖
#include "U8g2lib.h"
#include "freertos.h"
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
#define BLINKER_PRINT Serial // Blinker.h依赖
#define BLINKER_WIFI         // Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#include "HTTPClient.h"
#include "Blinker.h"
#include "FastLED.h"
#include "WiFiUdp.h"
#include "esp_task_wdt.h" //下面是和风天气的api,api的key手动再申请罢,一天3000次
#define URL "https://devapi.qweather.com/v7/weather/now?location=xxx&key=xxx&gzip=n"
#define URL2 "https://devapi.qweather.com/v7/air/now?location=xxx&key=xxx&gzip=n"
#define SSID ""
#define PASSWORD ""
#define AUTH_KEY ""
//定义五行后把下面的include去掉
#include "password.cpp"
#include "esp_heap_caps.h"
////////////////////////////////////////////////////////////////
//灯光初始化定义
#define NUM_LEDS 120
#define DATA_PIN 25
#define USE_MULTCORE 1
//临界互斥锁,保护leds资源
static portMUX_TYPE leds_mutex = portMUX_INITIALIZER_UNLOCKED;
CRGB leds[NUM_LEDS];
CRGB leds_rgb_mode[NUM_LEDS];
TaskHandle_t rgb_run;
TaskHandle_t sitclock_run;
TaskHandle_t oled_run;
TaskHandle_t blinker_run;
TaskHandle_t http_run;
TaskHandle_t udp_run;
TaskHandle_t rgbChange_run;
TaskHandle_t button_run;
TaskHandle_t fastled_run;
////////////////////////////////////////////////////////////////
//全局初始化
WiFiUDP Udp;
int8_t start_setup = 111;
struct tm timeinfo;      //时间信息
int retry = 0;           //记录重试次数,全局变量
const char *ssid = SSID; //定义一个字符串(指针定义法)
const char *password = PASSWORD;
const char *auth = AUTH_KEY;
const char *ntpServer = "cn.ntp.org.cn"; //时间服务器
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/13, /* data=*/14, /* reset=*/U8X8_PIN_NONE); //定义u8g2
////////////////////////////////////////////////////////////////
// blinker注册
char b_name[32] = "btn-abc";
char n_name[32] = "num-abc";
char r_name[32] = "col-6ok";
BlinkerButton Button1(b_name);
BlinkerNumber Number1(n_name);
BlinkerRGB RGB1(r_name);
////////////////////////////////////////////////////////////////
//灯光状态部分,字面意思
int8_t oled_state = 1;                             //通信量,显示屏开关
int8_t oled_mode = 1;                              //通信量,显示屏模式 1 正常2 欢迎 3 久坐
int8_t mode = 0;                                   //通信量,灯光改变模式,并非小爱指定的模式,
int8_t light_on = 0;                               //小爱指定的开关状态,用于回调,与逻辑耦合的不是那么深,默认关
int8_t mi_mode = 0;                                //小爱指定的模式,用于回调,与逻辑耦合的不是那么深,默认日光
int mi_light_bright = 100;                         //小爱指定的亮度,用于回调,与逻辑耦合的不是那么深,默认100
int32_t light_now = (135 * 256 + 206) * 256 + 235; //小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
int8_t light_change = 0;                           //信号量,控制灯光
int8_t rgb_running = 0;                            //信号量,控制流光溢彩
int light_brightness = 255;                        //通信量
int light_now_brightness = 255;
int light_color_r[3]; //通信量
int light_color_g[3]; //通信量
int light_color_b[3]; //通信量
class LightSet
{
    int light_set_num = 0; //通信量
public:
    void reset()
    {
        light_set_num = 0;
    }
    void next()
    {
        light_set_num++;
        if (light_set_num == 3)
        {
            light_set_num = 0;
        }
    }
    int get_num()
    {
        return light_set_num;
    }
};
////////////////////////////////////////////////////////////////
LightSet light_set;
////////////////////////////////////////////////////////////////
// http请求部分,查天气,get
//
//#define ARDUINOJSON_USE_LONG_LONG 1
char text_final[30] = "   ";
char covid_final[30] = " ";
char temp_final[10] = " ";
char humidity_final[10] = " ";
char aqi_final[10] = " ";
char category_final[30] = " ";
char hitokoto_final[100] = "松花酿酒,春水煎茶。";
////////////////////////////////////////////////////////////////
//函数预定义部分
void oled_show(const char *str1, const char *str2, const char *str3, const char *str4); //提供三行英文输出
void rgb_task_shutdown();
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
        // target_hour = timeinfo.tm_hour;
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
// void sitclock_task(void *sitclock_task_pointer);
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
        if (is_sitclock() == 1 && rgb_running == 0)
        {
            printf("sit clock warning!!!");
            //久坐之后的操作
            mode = 4;
            light_change = 1;
            oled_mode = 3;
            blink_time = 3;
            reset_sitclock_limit();
        }
        else if (is_sitclock() == 1 && rgb_running == 1)
        {
            printf("sit clock warning!!!");
            //久坐之后的操作
            mode = 5;
            light_change = 1;
            oled_mode = 3;
            blink_time = 3;
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
// eeprom掉电保护部分
void EEPROM_rgb_commit()
{
    if (!EEPROM.begin(5120))
    {
        Serial.println("eeprom fail!");
        oled_show("smart_screen", "eeprom错误", "请等待", "正在重启...");
        delay(100);
        esp_restart();
    } //自定义从3000开始
    // Serial.printf("eeprom rgb commit check!\nrgb:%d:%d:%d\n", EEPROM.readInt(3000), EEPROM.readInt(3004), EEPROM.readInt(3008));
    EEPROM.writeInt(3000, light_color_r[0]);
    EEPROM.writeInt(3004, light_color_g[0]);
    EEPROM.writeInt(3008, light_color_b[0]);
    EEPROM.writeInt(3012, light_color_r[1]);
    EEPROM.writeInt(3016, light_color_g[1]);
    EEPROM.writeInt(3020, light_color_b[1]);
    EEPROM.writeInt(3024, light_color_r[2]);
    EEPROM.writeInt(3028, light_color_g[2]);
    EEPROM.writeInt(3032, light_color_b[2]);
    EEPROM.commit();
    Serial.printf("eeprom rgb commit success!\n");
    Serial.printf("rgb1:%d:%d:%d", light_color_r[0], light_color_g[0], light_color_b[0]);
    Serial.printf("rgb2:%d:%d:%d", light_color_r[1], light_color_g[1], light_color_b[1]);
    Serial.printf("rgb3:%d:%d:%d", light_color_r[2], light_color_g[2], light_color_b[2]);
    EEPROM.end();
}
void EEPROM_setup()
{
    if (!EEPROM.begin(5120))
    {
        Serial.println("eeprom fail!");
        oled_show("smart_screen", "eeprom错误", "请等待", "正在重启...");
        delay(100);
        esp_restart();
    } //自定义从3000开始
    for (int i = 3000; i <= 3032; i = i + 4)
    {
        if (EEPROM.readInt(i) == -1)
        {
            EEPROM.writeInt(i, 255);
            break;
        }
    }
    light_color_r[0] = EEPROM.readInt(3000);
    light_color_g[0] = EEPROM.readInt(3004);
    light_color_b[0] = EEPROM.readInt(3008);
    light_color_r[1] = EEPROM.readInt(3012);
    light_color_g[1] = EEPROM.readInt(3016);
    light_color_b[1] = EEPROM.readInt(3020);
    light_color_r[2] = EEPROM.readInt(3024);
    light_color_g[2] = EEPROM.readInt(3028);
    light_color_b[2] = EEPROM.readInt(3032);
    Serial.printf("rgb1:%d:%d:%d", light_color_r[0], light_color_g[0], light_color_b[0]);
    Serial.printf("rgb2:%d:%d:%d", light_color_r[1], light_color_g[1], light_color_b[1]);
    Serial.printf("rgb3:%d:%d:%d", light_color_r[2], light_color_g[2], light_color_b[2]);
    light_now = (light_color_r[0] * 256 + light_color_g[0]) * 256 + light_color_b[0]; //小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
    // Serial.printf("p:%d\n", EEPROM.readInt(3000));
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
        // String responsePayload = httpClient3.getString();
        const String &payload3 = httpClient3.getString();
        Serial.println("Server Response Payload:");
        Serial.println(payload3);
        DynamicJsonDocument jsonBuffer3(4096);
        deserializeJson(jsonBuffer3, payload3);
        JsonObject root3 = jsonBuffer3.as<JsonObject>();
        // JsonArray now = root["now"];
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
void esp32_Http_2()
{
    //创建 HTTPClient 对象
    HTTPClient httpClient;

    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient.begin(URL2);
    httpClient.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    httpClient.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode = httpClient.GET();
    Serial.print("Send GET request to URL: ");
    Serial.println(URL2);

    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode == HTTP_CODE_OK)
    {
        // String responsePayload = httpClient.getString();
        // const String &payload = httpClient.getString();
        Serial.println("Server Response Payload:");
        // Serial.println(payload);
        DynamicJsonDocument jsonBuffer(2048);
        deserializeJson(jsonBuffer, httpClient.getStream());
        JsonObject root = jsonBuffer.as<JsonObject>();
        // JsonArray now = root["now"];
        const char *category = root["now"]["category"];
        const char *aqi = root["now"]["aqi"];
        if (category != NULL && aqi != NULL)
        {
            sprintf(aqi_final, "%s", aqi);
            sprintf(category_final, "%s", category);
        }
        Serial.println("url2 get");
    }
    else
    {
        Serial.println("Server Respose Code:");
        Serial.println(httpCode);
    }
    //关闭与服务器连接
    httpClient.end();
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
        // String responsePayload = httpClient.getString();
        // const String &payload = httpClient.getString();
        Serial.println("Server Response Payload:");
        // Serial.println(payload);
        DynamicJsonDocument jsonBuffer(2048);
        deserializeJson(jsonBuffer, httpClient.getStream());
        JsonObject root = jsonBuffer.as<JsonObject>();
        // JsonArray now = root["now"];
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
        // String responsePayload = httpClient.getString();
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
    if (oled_state == 1)
    {
        if (oled_mode == 1)
        {
            // char str_sum[100];//不需要日志注释掉
            // char *str = &str_sum[0];//不需要日志注释掉
            u8g2.clearBuffer();
            // u8g2.setFont(u8g2_font_ncenB12_tr);
            u8g2.setFont(u8g2_font_wqy16_t_gb2312);
            // u8g2.drawStr(0, 12, str1);
            // u8g2.drawStr(0, 27, str2);
            // u8g2.drawStr(0, 42, str3);
            //  u8g2.drawStr(0, 57, str4);
            u8g2.setCursor(0, 13);
            u8g2.print(str1);
            u8g2.setCursor(0, 30);
            u8g2.print(str2);
            u8g2.setCursor(0, 47);
            u8g2.print(str3);
            u8g2.setFont(u8g2_font_wqy14_t_gb2312);
            u8g2.setCursor(0, 62);
            u8g2.print(str4);
            // sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
            // Serial.println(str);//不需要日志注释掉
            Serial.printf("oled_change:");
            Serial.printf(str1);
            Serial.printf(str2);
            Serial.printf(str3);
            Serial.printf(str4);
            Serial.println();
            u8g2.sendBuffer();
        }
        else if (oled_mode == 2)
        {
            // char str_sum[100];//不需要日志注释掉
            // char *str = &str_sum[0];//不需要日志注释掉
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
            // sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
            // Serial.println(str);//不需要日志注释掉
            Serial.println("oled_change");
            u8g2.sendBuffer();
            delay(700);
            oled_mode = 1; //欢迎完正常
        }
        else if (oled_mode == 3)
        {
            // char str_sum[100];//不需要日志注释掉
            // char *str = &str_sum[0];//不需要日志注释掉
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
            // sprintf(str, "oled_showing:\n%s\n%s\n%s\n", str1, str2, str3);//不需要日志注释掉
            // Serial.println(str);//不需要日志注释掉
            Serial.println("oled_change");
            u8g2.sendBuffer();
            delay(10000);
            oled_mode = 1; //提示完正常
        }
    }
    else if (oled_state == 0)
    {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        Serial.println("oled_off");
    }
}
void print_oled() //常驻显示任务,必须循环,否则出事
{
    if (!(getLocalTime(&timeinfo) && WiFi.status() == WL_CONNECTED)) //获取时间不成功(一次也没)...允悲
    {
        // oled_show("error:404", "pls wait", "retrying...", "");
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
        if (retry == 7)
        {
            Serial.println("error:no connect");
            oled_show("smart_screen", "系统错误", "网络寄了", "正在重启...");
            esp_restart();
        }
        return;
    }
    if (start_setup == 111)
    {
        start_setup = 1;
    }
    char str1[60];
    char str2[60];
    char str3[60];
    char st[60];
    char strrr[60];
    strftime(st, 100, "%a", &timeinfo);
    sprintf(str1, "%4d-%02d-%02d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, st); //整合字符串
    strftime(strrr, 100, "%H:%M:%S", &timeinfo);
    if (rgb_running == 0)
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
    // sprintf(str2, "%s %s", strrr, text_final);
    if (timeinfo.tm_sec % 10 >= 5)
    {
        sprintf(str3, "%s|%s℃ %s", text_final, temp_final, aqi_final);
    }
    else
    {
        sprintf(str3, "%s|%s%% %s", text_final, humidity_final, category_final);
    }

    oled_show(str1, str2, str3, hitokoto_final);
    /*if (ok == 0)
    w{
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        ok = 1;
    }*/
    // Serial.println(&timeinfo, "%F %T %A");//日志
}
int counter = 0;       //官方计数
int rgb_screen_on = 0; //逻辑上 rgb_screen ~= light_change 但light>rgb>mode
int rgb_frist_running = 0;
void button1_callback(const String &state)
{
    //./././
}
void rgb1_callback(uint8_t r_value, uint8_t g_value, uint8_t b_value, uint8_t bright_value)
{
    rgb_task_shutdown();
    light_on = 1; //强制开启
    light_now = (r_value * 256 + g_value) * 256 + b_value;
    Serial.println("color change");
    light_color_r[light_set.get_num()] = r_value;
    light_color_g[light_set.get_num()] = g_value;
    light_color_b[light_set.get_num()] = b_value;
    EEPROM_rgb_commit();
    light_set.next();
    Serial.printf("color:%d:%d:%d||%d\n", light_color_r[light_set.get_num()], light_color_g[light_set.get_num()], light_color_b[light_set.get_num()], light_set.get_num());
    on_sitclock();
    light_change = 1;
    mode = 3;
}
void dataRead(const String &data)
{
    BLINKER_LOG("Blinker readString: ", data);

    Blinker.vibrate();

    uint32_t BlinkerTime = millis();

    Blinker.print("millis", BlinkerTime);
}

uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;
void rgb_screen()
{
    // Serial.println("rgb_show");
    //  Wait for first byte of Magic Word
    //魔法包结构 Ada+校验码+rgb数据
    for (i = 0; i < sizeof prefix; ++i) //读到Ada开始
    {
    waitLoop:
        // delay(1); // otherwise, start over
        while (!Serial.available())
            ;
        ;
        // Check next byte in Magic Word
        if (prefix[i] == Serial.read())
            continue;
        i = 0;
        goto waitLoop;
    }
    // Hi, Lo, Checksum
    while (!Serial.available())
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
    } //线程阻断
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        byte r, g, b;
        while (!Serial.available())
            ;
        r = Serial.read();
        while (!Serial.available())
            ;
        g = Serial.read();
        while (!Serial.available())
            ;
        b = Serial.read();
        leds_rgb_mode[i].r = r;
        leds_rgb_mode[i].g = g;
        leds_rgb_mode[i].b = b;
    }
    portENTER_CRITICAL(&leds_mutex);
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        leds[i].r = leds_rgb_mode[i].r;
        leds[i].g = leds_rgb_mode[i].g;
        leds[i].b = leds_rgb_mode[i].b;
    }
    portEXIT_CRITICAL(&leds_mutex);
    // Shows new value
    // FastLED.show();
    // sprintf(aqi_final, "sec");
}
void rgbScreenTask(void *xTaskRgbScreen) //流光溢彩任务
{
    while (1)
    {
        rgb_screen();
    }
}
void rgb_task_run()
{
    rgb_running = 1;
    if (rgb_frist_running == 0)
    {
        xTaskCreatePinnedToCore(rgbScreenTask, "Taskrgb", 4096, NULL, 2, &rgb_run, 1);
        rgb_frist_running = 1;
    }
    else
    {
        vTaskResume(rgb_run);
    }
}
void rgb_task_shutdown()
{
    if (rgb_running == 1)
    {
        vTaskSuspend(rgb_run);
    }
    rgb_running = 0;
}
void miotPowerState(const String &state)
{
    BLINKER_LOG("need set power state: ", state);

    if (state == BLINKER_CMD_ON)
    {
        light_on = 1;
        oled_state = 1;
        oled_mode = 2;
        Serial.println("light on");
        mode = 1; //默认日光
        on_sitclock();
        delay(150);
        light_change = 1;
        // oled_show("", "", "light on");
        BlinkerMIOT.powerState("on");
        BlinkerMIOT.print();
    }
    else if (state == BLINKER_CMD_OFF)
    {
        // oled_show("", "", "light off");
        light_set.reset();
        rgb_task_shutdown();
        mode = 0;
        Serial.println("light off");
        delay(150);
        light_change = 1;
        oled_state = 0;
        light_on = 0;
        off_sitclock();

        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
}
void miotColor(int32_t color)
{
    rgb_task_shutdown();
    BLINKER_LOG("need set color: ", color);
    light_on = 1; //强制开启
    light_now = color;
    uint8_t colorR = color >> 16 & 0xFF;
    uint8_t colorG = color >> 8 & 0xFF;
    uint8_t colorB = color & 0xFF;
    Serial.printf("set color: %d\n", color);
    Serial.println("color change");
    BLINKER_LOG("colorR: ", colorR, ", colorG: ", colorG, ", colorB: ", colorB);
    light_color_r[light_set.get_num()] = colorR;
    light_color_g[light_set.get_num()] = colorG;
    light_color_b[light_set.get_num()] = colorB;
    if (light_color_r[light_set.get_num()] == 255 && light_color_g[light_set.get_num()] == 255 && light_color_b[light_set.get_num()] == 0)
    {
        light_color_r[light_set.get_num()] = 255;
        light_color_g[light_set.get_num()] = 150;
        light_color_b[light_set.get_num()] = 50;
    }
    EEPROM_rgb_commit();
    light_set.next();
    Serial.printf("color:%d:%d:%d||%d\n", light_color_r[light_set.get_num()], light_color_g[light_set.get_num()], light_color_b[light_set.get_num()], light_set.get_num());
    on_sitclock();
    light_change = 1;
    mode = 3;
    BlinkerMIOT.color(color);
    BlinkerMIOT.print();
    // oled_show("", "color", "change");
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
    // oled_show("", "brightness", "change");
}
void miotMode(uint8_t mode_mi)
{
    BLINKER_LOG("need set mode: ", mode_mi);
    mi_mode = mode_mi;
    if (mode_mi == BLINKER_CMD_MIOT_DAY)
    {
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        Serial.println("Set day mode");
        light_change = 1;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_NIGHT)
    {
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        for (int i = 0; i < 3; i++)
        {
            light_color_r[i] = 0;
            light_color_g[i] = 0;
            light_color_b[i] = 255;
        }

        EEPROM_rgb_commit();
        Serial.print("mi_color||r:");
        Serial.print(light_color_r[0]);
        Serial.print("  g:");
        Serial.print(light_color_g[0]);
        Serial.print("  b:");
        Serial.println(light_color_b[0]);
        light_brightness = 120;
        light_change = 1;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COLOR)
    {
        // Your mode function
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        portENTER_CRITICAL(&leds_mutex);
        for (int i = 0; i < 48; i++)
        {
            leds[i].r = 255;
            leds[i].g = 0;
            leds[i].b = 0;
        }
        for (int i = 0; i < 48; i++)
        {
            leds[i + 48].r = 0;
            leds[i + 48].g = 0;
            leds[i + 48].b = 255;
        }
        for (int i = 0; i < 24; i++)
        {
            leds[i + 96].r = 255;
            leds[i + 96].g = 0;
            leds[i + 96].b = 255;
        }
        portEXIT_CRITICAL(&leds_mutex);
        light_color_r[0] = 255; //通信量,上灯带
        light_color_g[0] = 0;   //通信量
        light_color_b[0] = 0;   //通信量
        light_color_r[1] = 0;   //通信量,下灯带
        light_color_g[1] = 0;   //通信量
        light_color_b[1] = 255; //通信量
        light_color_r[2] = 255; //通信量,侧灯带
        light_color_g[2] = 0;   //通信量
        light_color_b[2] = 255; //通信量
        EEPROM_rgb_commit();
        Serial.print("mi_color||r:");
        Serial.print(light_color_r[0]);
        Serial.print("  g:");
        Serial.print(light_color_g[0]);
        Serial.print("  b:");
        Serial.println(light_color_b[0]);
        light_brightness = 120;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_WARMTH)
    {
        // Your mode function
    }
    else if (mode_mi == BLINKER_CMD_MIOT_TV)
    {
        rgb_task_shutdown();
        Udp.beginPacket("255.255.255.255", 8080); //配置远端ip地址和端口
        Udp.print("turn_off");                    //把数据写入发送缓冲区
        Udp.endPacket();                          //发送数据
        Serial.println("UDP数据发送成功");
        // oled_show("", "", "light off");
        Serial.println("light off");
        off_sitclock();
        delay(4000);
        light_change = 1;
        mode = 0;
        oled_state = 0;
        light_on = 0;
        //向udp工具发送消息

        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
    else if (mode_mi == BLINKER_CMD_MIOT_READING)
    {

        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        for (int i = 0; i < 3; i++)
        {
            light_color_r[i] = 255;
            light_color_g[i] = 150;
            light_color_b[i] = 50;
        }
        EEPROM_rgb_commit();
        Serial.print("mi_color||r:");
        Serial.print(light_color_r[0]);
        Serial.print("  g:");
        Serial.print(light_color_g[0]);
        Serial.print("  b:");
        Serial.println(light_color_b[0]);
        light_change = 1;
        light_brightness = 255;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COMPUTER)
    {
        light_on = 1;
        oled_state = 1;
        rgb_screen_on = 1;
        light_change = 1;
        Serial.print("Ada\n");
        on_sitclock();
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
        // BlinkerMIOT.colorTemp(1000);
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
        // BlinkerMIOT.colorTemp(1000);
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
        // BlinkerMIOT.colorTemp(1000);
        BlinkerMIOT.brightness(mi_light_bright);
        BlinkerMIOT.print();
        break;
    }
}
void oledTask(void *xTaskOled) //显示屏任务
{
    while (1)
    {
        print_oled();
        delay(300);
    }
}
void blinkerTask(void *xTaskBlinker) // blinker任务
{
    while (1)
    {
        Blinker.run();
        delay(10);
    }
}
void httpTask(void *xTaskHttp) //巨型http请求模块任务
{
    while (1)
    {
        delay(200);
        if (start_setup == 1)
        {
            esp32_Http();
            esp32_Http_2();
            esp32_Http_hitokoto();
            // esp32_Http_covid();
            start_setup = 0;
        }
        if (start_setup == 0)
        {
            if (timeinfo.tm_min % 3 == 0 && timeinfo.tm_sec == 0)
            {
                esp32_Http();
            }
            if (timeinfo.tm_min % 3 == 1 && timeinfo.tm_sec == 0)
            {
                esp32_Http_2();
            }
            if (timeinfo.tm_min % 3 == 2 && timeinfo.tm_sec == 0)
            {
                esp32_Http_hitokoto();
            }
        }
    }
}
void udpTask(void *xTaskUdp)
{
    while (1)
    {
        int packetSize = Udp.parsePacket(); //获得解析包
        if (packetSize)                     //解析包不为空
        {
            //收到Udp数据包
            // Udp.remoteIP().toString().c_str()用于将获取的远端IP地址转化为字符串
            Serial.printf("收到来自远程IP：%s（远程端口：%d）的数据包字节数：%d\n", Udp.remoteIP().toString().c_str(), Udp.remotePort(), packetSize);
            char incomingPacket[255];
            // 读取Udp数据包并存放在incomingPacket
            int len = Udp.read(incomingPacket, 255); //返回数据包字节数
            if (len > 0)
            {
                incomingPacket[len] = 0; //清空缓存
                if (strcmp(incomingPacket, "turn_on") == 0)
                {
                    miotPowerState("on");
                }
                else if (strcmp(incomingPacket, "turn_off") == 0)
                {
                    miotPowerState("off");
                }
                else if (strcmp(incomingPacket, "computer") == 0)
                {
                    Serial.println("computer mode");
                    miotMode((uint8_t)6);
                }
                Serial.printf("UDP数据包内容为: %s\n", incomingPacket); //向串口打印信息
            }
        }
        delay(3000); //延时3秒
    }
}
void buttonTask(void *xTaskButton)
{
    uint8_t button2_pin = 34;
    uint8_t button1_pin = 35;
    int button1_now = 0;
    int button2_now = 0;
    int button1_before = 0;
    int button2_before = 0;
    while (1)
    {
        // Serial.printf("pin1:%d;", digitalRead(button1_pin));
        // Serial.printf("pin2:%d;\n", digitalRead(button2_pin));
        button1_now = digitalRead(button1_pin);
        button2_now = digitalRead(button2_pin);
        if (button1_now == 0 && button1_before == 1)
        {
            Serial.printf("\n1up\n");
        }
        else if (button1_now == 1 && button1_before == 0)
        {
            Serial.printf("\n1down\n");
            if (light_on == 1)
            {
                miotPowerState("off");
            }
            else if (light_on == 0)
            {
                miotPowerState("on");
            }
        }
        if (button2_now == 0 && button2_before == 1)
        {
            Serial.printf("\n2up\n");
        }
        else if (button2_now == 1 && button2_before == 0)
        {
            if (mi_mode == BLINKER_CMD_MIOT_DAY)
            {
                miotMode(BLINKER_CMD_MIOT_COMPUTER);
            }
            else if (mi_mode == BLINKER_CMD_MIOT_COMPUTER)
            {
                miotMode(BLINKER_CMD_MIOT_DAY);
            }
            else
            {
                miotMode(BLINKER_CMD_MIOT_DAY);
            }
            Serial.printf("\n2down\n");
        }
        button1_before = button1_now;
        button2_before = button2_now;
        delay(10);
    }
}
void fastledTask(void *xTaskfastled)
{
    while (1)
    {
        FastLED.show();
        delay(10);
    }
}
void light_color_out(int *r, int *g, int *b, int bright)
{
    CRGB light_change_color[3];
    int light_change_brightness = 0;
    CRGB light_now[3];
    int r_all[3];
    int g_all[3];
    int b_all[3];
    for (int i = 0; i < 3; i++)
    {
        r_all[i] = 0;
        g_all[i] = 0;
        b_all[i] = 0;
    }
    portENTER_CRITICAL(&leds_mutex);
    for (int i = 0; i < 48; i++)
    {
        r_all[0] = leds[i].r + r_all[0];
        g_all[0] = leds[i].g + g_all[0];
        b_all[0] = leds[i].b + b_all[0];
    }
    for (int i = 0; i < 48; i++)
    {
        r_all[1] = leds[i + 48].r + r_all[1];
        g_all[1] = leds[i + 48].g + g_all[1];
        b_all[1] = leds[i + 48].b + b_all[1];
    }
    for (int i = 0; i < 24; i++)
    {
        r_all[2] = leds[i + 96].r * 2 + r_all[2];
        g_all[2] = leds[i + 96].g * 2 + g_all[2];
        b_all[2] = leds[i + 96].b * 2 + b_all[2];
    }
    portEXIT_CRITICAL(&leds_mutex);
    for (int i = 0; i < 3; i++)
    {
        light_now[i].r = r_all[i] / 48;
        light_now[i].g = g_all[i] / 48;
        light_now[i].b = b_all[i] / 48;
    }
    Serial.printf("%d:%d:%d\n", light_now[i].r, light_now[i].g, light_now[i].b);
    for (int8_t n = 1; n <= 24; n++)
    {
        for (int i = 0; i < 3; i++)
        {
            light_change_color[i].r = light_now[i].r + (r[i] - light_now[i].r) * n / 24;
            light_change_color[i].g = light_now[i].g + (g[i] - light_now[i].g) * n / 24;
            light_change_color[i].b = light_now[i].b + (b[i] - light_now[i].b) * n / 24;
            light_change_brightness = light_now_brightness + (bright - light_now_brightness) * n / 24;
        }

        portENTER_CRITICAL(&leds_mutex);
        for (int i = 0; i < n; i++)
        {
            leds[23 - i].r = light_change_color[0].r;
            leds[23 - i].g = light_change_color[0].g;
            leds[23 - i].b = light_change_color[0].b;
            leds[24 + i].r = light_change_color[0].r;
            leds[24 + i].g = light_change_color[0].g;
            leds[24 + i].b = light_change_color[0].b;
            leds[71 - i].r = light_change_color[1].r;
            leds[71 - i].g = light_change_color[1].g;
            leds[71 - i].b = light_change_color[1].b;
            leds[72 + i].r = light_change_color[1].r;
            leds[72 + i].g = light_change_color[1].g;
            leds[72 + i].b = light_change_color[1].b;
            leds[96 + i].r = light_change_color[2].r;
            leds[96 + i].g = light_change_color[2].g;
            leds[96 + i].b = light_change_color[2].b;
        }
        light_change = 0;
        portEXIT_CRITICAL(&leds_mutex);
        FastLED.setBrightness(light_change_brightness);
        Serial.println("rgb change");
        delay(10);
    }
    light_now_brightness = light_brightness;
}
void rgbChangeTask(void *xTaskRgbChange) //灯条任务
{
    while (1)
    {
        if (light_change == 1)
        {
            if (rgb_running == 0) // 1 全色
            {
                // Move a single white led
                if (rgb_screen_on == 1)
                {
                    rgb_screen_on = 0;
                    // rgb_running = 1;
                    rgb_task_run();
                    Serial.println("light");
                    // off_sitclock();
                    // xTaskCreatePinnedToCore(rgbScreenTask, "Taskrgb", 4096, NULL, 2, &rgb_run, 1);
                    mode = 2;
                    light_change = 0;
                    continue;
                }
                if (mode == 1 || mode == 3) //换亮度/色彩/////mode2悬空给rgb
                {
                    int r[3], g[3], b[3];
                    for (int i = 0; i < 3; i++)
                    {
                        r[i] = light_color_r[i];
                        g[i] = light_color_g[i];
                        b[i] = light_color_b[i];
                    }
                    light_color_out(r, g, b, light_brightness);
                    Serial.println("led on");
                }
                else if (mode == 0) //关led
                {
                    int r[3], g[3], b[3];
                    for (int i = 0; i < 3; i++)
                    {
                        r[i] = 0;
                        g[i] = 0;
                        b[i] = 0;
                    }
                    light_color_out(r, g, b, light_brightness);
                    Serial.println("led on");
                }
                else if (mode == 4) //久坐闪灯
                {
                    if (blink_time > 0)
                    {
                        int r1[3], g1[3], b1[3];
                        for (int i = 0; i < 3; i++)
                        {
                            r1[i] = 255;
                            g1[i] = 0;
                            b1[i] = 0;
                        }
                        light_color_out(r1, g1, b1, light_brightness);
                        Serial.println("led blink");
                        delay(1000);
                        int r[3];
                        int g[3];
                        int b[3];
                        r[0] = light_color_r[0];
                        g[0] = light_color_g[0];
                        b[0] = light_color_b[0];
                        r[1] = light_color_r[1];
                        g[1] = light_color_g[1];
                        b[1] = light_color_b[1];
                        r[2] = light_color_r[2];
                        g[2] = light_color_g[2];
                        b[2] = light_color_b[2];
                        light_color_out(r, g, b, light_brightness);
                        delay(1000);
                        Serial.println("led blink");
                        blink_time--;
                    }
                    else
                    {
                        light_change = 0;
                        mode = 1;
                        Serial.println("blink failed");
                    }
                }
                else if (mode == 5)
                {
                    mode = 2;
                }
            }
            else if (rgb_running == 1 && mode == 5) // usb模式久坐闪灯
            {
                if (blink_time > 0)
                {
                    vTaskSuspend(rgb_run);
                    portENTER_CRITICAL(&leds_mutex);
                    for (int i = 0; i < 120; i++)
                    {
                        leds[i].r = 255;
                        leds[i].g = 0;
                        leds[i].b = 0;
                    }
                    portEXIT_CRITICAL(&leds_mutex);
                    // FastLED.show();
                    delay(1000);
                    vTaskResume(rgb_run);
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
        delay(100);
    }
}
void setup()
{
    Serial.begin(460800);
    Serial.println("bszydxh esp32 start!");
    WiFi.mode(WIFI_STA);
    // BLEDevice::init("esp32");
    //  pBLEScan = BLEDevice::getScan(); //create new scan
    //  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    //  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
    //  pBLEScan->setInterval(1000);
    //  pBLEScan->setWindow(99); // less or equal setInterval value
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
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
            oled_show("smart_screen", "系统错误", "wifi寄了", "正在重启...");
            esp_restart();
        }
    }
    retry = 0;
    oled_show("smart_screen", "---bszydxh", "连接成功", "加载系统中...");
    Serial.println("wifi! done");
    if (Udp.begin(1145))
    { //启动Udp监听服务
        Serial.println("监听成功");
        //打印本地的ip地址，在UDP工具中会使用到
        // WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
        Serial.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), 1145);
    }
    else
    {
        Serial.println("监听失败");
    }
    EEPROM_setup();
    Blinker.begin(auth, ssid, password);
    Blinker.attachData(dataRead);
    Button1.attach(button1_callback);
    RGB1.attach(rgb1_callback);
    BLINKER_DEBUG.stream(Serial);
    BLINKER_DEBUG.debugAll();
    BlinkerMIOT.attachPowerState(miotPowerState);
    BlinkerMIOT.attachColor(miotColor);
    BlinkerMIOT.attachMode(miotMode);
    BlinkerMIOT.attachBrightness(miotBright);
    BlinkerMIOT.attachQuery(miotQuery);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    light_change = 1;
    pinMode(34, INPUT);
    pinMode(35, INPUT);
    xTaskCreatePinnedToCore(oledTask, "oledTask", 2048, NULL, 1, &oled_run, 0);
    xTaskCreatePinnedToCore(blinkerTask, "blinkerTask", 7168, NULL, 2, &blinker_run, 0);
    xTaskCreatePinnedToCore(httpTask, "httpTask", 7168, NULL, 0, &http_run, 0);
    xTaskCreatePinnedToCore(udpTask, "udpTask", 4096, NULL, 0, &udp_run, 0);
    xTaskCreatePinnedToCore(rgbChangeTask, "rgbChangeTask", 2048, NULL, 3, &rgbChange_run, 1); //请不要动,动了就寄
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 4096, NULL, 2, &button_run, 0);
    xTaskCreatePinnedToCore(fastledTask, "fastledTask", 2048, NULL, 3, &fastled_run, 1);
}
void loop()
{
    // Blinker.run(); //wifi blinker自动处理 不用管
    // light();
    // Serial.printf("Freeheap:%d\n", xPortGetFreeHeapSize());
    // Serial.printf("FreeMinheap:%d\n", xPortGetMinimumEverFreeHeapSize());
    // Serial.println("///////////////////////////////////");
    // Serial.println("rgb");
    // Serial.println(uxTaskGetStackHighWaterMark(rgb_run));
    // Serial.println("sit");
    // Serial.println(uxTaskGetStackHighWaterMark(sitclock_run));
    // Serial.println("oled");
    // Serial.println(uxTaskGetStackHighWaterMark(oled_run));
    // Serial.println("blinker");
    // Serial.println(uxTaskGetStackHighWaterMark(blinker_run));
    // Serial.println("http");
    // Serial.println(uxTaskGetStackHighWaterMark(http_run));
    // Serial.println("udp");
    // Serial.println(uxTaskGetStackHighWaterMark(udp_run));
    // Serial.println("rgbc");
    // Serial.println(uxTaskGetStackHighWaterMark(rgbChange_run));
    // Serial.println("button");
    // Serial.println(uxTaskGetStackHighWaterMark(button_run));
    // Serial.println("fastled");
    // Serial.println(uxTaskGetStackHighWaterMark(fastled_run));
    // Serial.println("/////////////////////////////////////////////");
    delay(500); //踢看门狗,loop本质上也是freertos中的一个任务
}