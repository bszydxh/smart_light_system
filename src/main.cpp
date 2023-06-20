/*
引脚配置:
13 -> 显示屏时钟信号(scl)
14 -> 显示屏数据信号(sda)
25 -> 灯带pwm信号
34 -> 按钮二
35 -> 按钮一
*/
/*
esp_EEPROM 0-1024自定义
////记忆灯光////
00 r1
04 g1
08 b1
12 r2
16 g2
20 b2
24 r3
28 g3
32 b3
36 亮度
////灯光挂掉重启记忆////
100
*/
/*系统由freertos接管*/
/*使用Espressif 32 platfromio 版本为4.3.0
#define CONFIG_FREERTOS_USE_TRACE_FACILITY
#define CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
/*delay()就是vTaskdelay(),不信自己跳转看一下*/
/*在freertos中应避免对sprintf进行调用*/
#include <Arduino.h>                  //主依赖,具体依赖见依赖树
#define FASTLED_ALL_PINS_HARDWARE_SPI // 强制规定fastled
#include "SPI.h"                      //U8g2.h依赖 Blinker.h依赖
#include "Wire.h"                     //U8g2.h依赖
#include "freertos/FreeRTOS.h"
#define DEST_FS_USES_LITTLEFS
#include "WiFi.h"             //Blinker.h依赖
#include "ESPmDNS.h"          //Blinker.h依赖
#include "FS.h"               //Blinker.h依赖
#include "SPIFFS.h"           //Blinker.h依赖
#include "Ticker.h"           //Blinker.h依赖
#include "Update.h"           //Blinker.h依赖
#include "WiFiClientSecure.h" //Blinker.h依赖
#include "EEPROM.h"           //Blinker.h依赖
#define BLINKER_PRINT Serial  // Blinker.h依赖
#define BLINKER_WIFI          // Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#define BLINKER_WITHOUT_SSL
#define BLINKER_NO_LOGO
#include "HTTPClient.h"
#include "FastLED.h"
#include "AsyncUDP.h"
#include "U8g2lib.h"
#include "esp_task_wdt.h" //下面是和风天气的api,api的key手动再申请罢,一天3000次
#include "Blinker.h"
#define HTTP_USERAGENT "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36"
#define WEATHER_URL "https://devapi.qweather.com/v7/weather/now?location=xxx&key=xxx&gzip=n"
#define AIR_URL "https://devapi.qweather.com/v7/air/now?location=xxx&key=xxx&gzip=n"
#define SSID ""
#define PASSWORD 0 // 也可以手动配置，就不用自动配网
#define AUTH_KEY ""
#define DEVICE_NAME "smart_light"
#define DEVICE_SUB_NAME "--bszydxh"
#define COMPUTER_PORT 8082
#define ANDROID_PORT 8081
#define ESP32_OLED_PORT 1145
#define ESP32_KEYBOARD_PORT 8484
#define MAX_SRV_CLIENTS 4
#if __has_include("config.cpp") // 非c++官方用法，config.cpp是项目作者自己的配置文件
#include "config.cpp"
#endif
#if __has_include("xd.h") // 非c++官方用法，xd.h是项目作者自己的魔改部分，看情况删
#include "xd.h"
#endif
#include "function.h"
// 定义五行后把下面 #include "config.cpp" 去掉
#include "esp_heap_caps.h"
#define DEBUG                   // 调试模式
#define ESPLOG_LEVEL ESPLOG_ALL // 调试等级
////////////////////////////////////////////////////////////////
// 灯光初始化定义
#define NUM_LEDS 120
#define DATA_PIN 25
#define USE_MULTCORE 1 // 使用双核
// 临界互斥锁,保护leds资源
//  static portMUX_TYPE leds_mutex = portMUX_INITIALIZER_UNLOCKED;
volatile int brightness_with_leds = 255; // 实时量
TaskHandle_t rgb_run;
TaskHandle_t sitclock_run;
TaskHandle_t oled_run;
TaskHandle_t blinker_run;
TaskHandle_t http_run;
TaskHandle_t udp_run;
TaskHandle_t tcp_run;
TaskHandle_t udp_config_run;
TaskHandle_t rgbChange_run;
TaskHandle_t button_run;
TaskHandle_t button_config_run;
TaskHandle_t fastled_run;
TaskHandle_t debug_run;
////////////////////////////////////////////////////////////////
// 全局初始化
WiFiUDP Udp;

#define CONFIG_SETUP 2
#define NOT_SETUP 1
#define FINISH_SETUP 0
QueueHandle_t system_state_queue;
QueueHandle_t leds_queue;
struct tm timeinfo;                        // 时间信息
int retry = 0;                             // 记录重试次数,全局变量
const char *ntpServer1 = "ntp.ntsc.ac.cn"; // 时间服务器1
const char *ntpServer2 = "cn.ntp.org.cn";  // 时间服务器2
const char *ntpServer3 = "ntp.aliyun.com"; // 时间服务器3
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/13, /* data=*/14); // 定义u8g2
////////////////////////////////////////////////////////////////
// blinker注册
char b_name[32] = "btn-abc";
char n_name[32] = "num-abc";
char r_name[32] = "col-6ok";
BlinkerButton Button1(b_name);
BlinkerNumber Number1(n_name);
BlinkerRGB RGB1(r_name);
////////////////////////////////////////////////////////////////
String eeprom_ssid;
String eeprom_pwd;
// 灯光状态部分,字面意思
volatile int8_t oled_state = 1;                             // 通信量,显示屏开关
volatile int8_t oled_mode = 1;                              // 通信量,显示屏模式 1 正常2 欢迎 3 久坐
volatile int8_t mode = 0;                                   // 通信量,灯光改变模式,并非小爱指定的模式,
volatile int8_t light_on = 0;                               // 小爱指定的开关状态,用于回调,与逻辑耦合的不是那么深,默认关
volatile int8_t mi_mode = 0;                                // 小爱指定的模式,用于回调,与逻辑耦合的不是那么深,默认日光
volatile int mi_light_bright = 100;                         // 小爱指定的亮度,用于回调,与逻辑耦合的不是那么深,默认100
volatile int32_t light_now = (135 * 256 + 206) * 256 + 235; // 小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
volatile int8_t light_change = 0;                           // 信号量,控制灯光
volatile int8_t rgb_running = 0;                            // 信号量,控制流光溢彩
volatile int light_brightness = 255;                        // 通信量
volatile int light_now_brightness = 255;
volatile int light_color_r[3]; // 通信量，好像可以压缩啊
volatile int light_color_g[3]; // 通信量
volatile int light_color_b[3]; // 通信量
volatile int time_hour = 0;    // 通信量
volatile int time_min = 0;     // 通信量
volatile int time_all = 0;     // 通信量

////////////////////////////////////////////////////////////////
LightSet light_set;
ESPLog esp_log;
////////////////////////////////////////////////////////////////
// http请求部分,查天气,get
// #define ARDUINOJSON_USE_LONG_LONG 1
char text_final[30] = "";
char covid_final[30] = "";
char temp_final[10] = "";
char humidity_final[10] = "";
char aqi_final[10] = "";
char category_final[30] = "";
char hitokoto_final[100] = "松花酿酒,春水煎茶。";
char xd_final[30] = "";
////////////////////////////////////////////////////////////////
// 函数预定义部分
void oled_show(const char *str1, const char *str2, const char *str3, const char *str4); // 提供三行英文输出
void rgb_task_shutdown();
////////////////////////////////////////////////////////////////
// 久坐提醒部分,默认1h
// 算是手写看门狗...咬自己...
int blink_time = 5; // 闪灯次数
int target_hour = -1;
int target_min = -1;
int sitclock_on = 0;  // 状态指示
void reset_sitclock() // 重置看门钟
{
    esp_log.println("set_clock");
    if (timeinfo.tm_hour == 23)
    {
        target_hour = 0;
    }
    else
    {
        target_hour = timeinfo.tm_hour + 1;
        // target_hour = timeinfo.tm_hour;
    } // 增加一个小时
    target_min = timeinfo.tm_min;
    time_hour = timeinfo.tm_hour;
    time_min = timeinfo.tm_min;
    time_all = 50;
    esp_log.printf("set_clock:%d:%d\n", target_hour, target_min);
}
void reset_sitclock_limit()
{
    target_hour = timeinfo.tm_hour;
    target_min = timeinfo.tm_min + 10;
    time_all = time_all + 10;
    if (target_min >= 60)
    {
        target_min = target_min - 60;
        target_hour++;
    }
    esp_log.printf("set_clock_limit:%d:%d\n", target_hour, target_min);
}
// void sitclock_task(void *sitclock_task_pointer);
bool off_sitclock() // 跟关灯绑定
{
    target_hour = -1;
    target_min = -1;
    if (sitclock_on != 0)
    {
        esp_log.println("sitclock off!");
        sitclock_on = 0;
        vTaskDelete(sitclock_run);
        return true;
    }
    return false;
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
        esp_log.info_printf("sit clock try \n");
        if (is_sitclock() == 1 && rgb_running == 0)
        {
            esp_log.info_printf("sit clock warning!!!\n");
            // 久坐之后的操作
            mode = 4;
            light_change = 1;
            oled_mode = 3;
            blink_time = (time_all - 40) / 10; // 动态增加闪灯次数
            reset_sitclock_limit();
        }
        else if (is_sitclock() == 1 && rgb_running == 1)
        {
            esp_log.info_printf("sit clock warning!!!\n");
            // 久坐之后的操作
            mode = 5;
            light_change = 1;
            oled_mode = 3;
            blink_time = (time_all - 40) / 10;
            reset_sitclock_limit();
        }
        delay(5000);
    }
}
void on_sitclock() // 跟开灯绑定(含类似行为)
{
    if (sitclock_on == 0)
    {
        reset_sitclock();
        xTaskCreatePinnedToCore(sitclock_task, "setclockTask", 2048, NULL, 0, &sitclock_run, 0);
    }
    sitclock_on = 1;
    esp_log.println("sitclock start!");
}
////////////////////////////////////////////////////////////////
// eeprom掉电保护部分
EEPROMClass esp_EEPROM("esp_EEPROM");
void EEPROM_rgb_memory_commit()
{
    int light_color_r_index[3];
    int light_color_g_index[3];
    int light_color_b_index[3];
    int light_brightness_index = light_brightness;
    for (int i = 0; i < 3; i++)
    {
        light_color_r_index[i] = light_color_r[i];
        light_color_g_index[i] = light_color_g[i];
        light_color_b_index[i] = light_color_b[i];
    }
    if (!esp_EEPROM.begin(1024))
    {
        esp_log.println("eeprom fail!");
        oled_show(DEVICE_NAME, "eeprom错误", "请等待", "正在重启...");
        delay(100);
        esp_restart();
    }
    esp_EEPROM.put(0, light_color_r_index[0]);
    esp_EEPROM.put(4, light_color_g_index[0]);
    esp_EEPROM.put(8, light_color_b_index[0]);
    esp_EEPROM.put(12, light_color_r_index[1]);
    esp_EEPROM.put(16, light_color_g_index[1]);
    esp_EEPROM.put(20, light_color_b_index[1]);
    esp_EEPROM.put(24, light_color_r_index[2]);
    esp_EEPROM.put(28, light_color_g_index[2]);
    esp_EEPROM.put(32, light_color_b_index[2]);
    esp_EEPROM.put(36, light_brightness_index);
    esp_EEPROM.commit();
    esp_log.printf("eeprom rgb commit success!\n");
    esp_log.printf("rgb1:%d:%d:%d\n", light_color_r_index[0], light_color_g_index[0], light_color_b_index[0]);
    esp_log.printf("rgb2:%d:%d:%d\n", light_color_r_index[1], light_color_g_index[1], light_color_b_index[1]);
    esp_log.printf("rgb3:%d:%d:%d\n", light_color_r_index[2], light_color_g_index[2], light_color_b_index[2]);
    esp_log.printf("bright:%d\n", light_brightness_index);
    esp_EEPROM.end();
}
void EEPROM_setup()
{
    if (!esp_EEPROM.begin(1024))
    {
        esp_log.error_printf("eeprom fail!\n");
        oled_show(DEVICE_NAME, "eeprom错误", "请等待", "正在重启...");
        delay(100);
        esp_restart();
    } // 自定义从3000开始
    for (int i = 0; i <= 36; i = i + 4)
    {
        if (esp_EEPROM.readInt(i) == -1)
        {
            esp_EEPROM.put(i, 255);
            break;
        }
    }
    esp_log.printf("?ssid: %s, pwd: %s\n", esp_EEPROM.readString(100), esp_EEPROM.readString(200));
    if (esp_EEPROM.readString(100).isEmpty())
    {
        esp_EEPROM.writeString(100, "unknown_ssid");
    }
    if (esp_EEPROM.readString(200).isEmpty())
    {
        esp_EEPROM.writeString(200, "unknown_pwd");
    }
    esp_EEPROM.commit();
    light_color_r[0] = esp_EEPROM.readInt(0);
    light_color_g[0] = esp_EEPROM.readInt(4);
    light_color_b[0] = esp_EEPROM.readInt(8);
    light_color_r[1] = esp_EEPROM.readInt(12);
    light_color_g[1] = esp_EEPROM.readInt(16);
    light_color_b[1] = esp_EEPROM.readInt(20);
    light_color_r[2] = esp_EEPROM.readInt(24);
    light_color_g[2] = esp_EEPROM.readInt(28);
    light_color_b[2] = esp_EEPROM.readInt(32);
    light_brightness = esp_EEPROM.readInt(36);
    eeprom_ssid = esp_EEPROM.readString(100);
    eeprom_pwd = esp_EEPROM.readString(200);
    esp_log.printf("ssid: %s, pwd: %s\n", eeprom_ssid, eeprom_pwd);
    esp_log.printf("rgb1:%d:%d:%d\n", light_color_r[0], light_color_g[0], light_color_b[0]);
    esp_log.printf("rgb2:%d:%d:%d\n", light_color_r[1], light_color_g[1], light_color_b[1]);
    esp_log.printf("rgb3:%d:%d:%d\n", light_color_r[2], light_color_g[2], light_color_b[2]);
    esp_log.printf("bright:%d\n", light_brightness);
    light_now = (light_color_r[0] * 256 + light_color_g[0]) * 256 + light_color_b[0]; // 小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
}
////////////////////////////////////////////////////////////////
int http_get(HTTPClient &http)
{
    // vTaskSuspend(blinker_run);
    int httpCode = http.GET();
    // vTaskResume(blinker_run);
    return httpCode;
}
void esp32_Http_aqi()
{
    // HTTPClient httpClient;
    // httpClient.begin(AIR_URL);
    // httpClient.setUserAgent(HTTP_USERAGENT);
    // httpClient.addHeader("charset", "utf-8");
    // int httpCode = http_get(httpClient);
    // esp_log.task_printf("esp32 -> aqi server\n");
    // if (httpCode == HTTP_CODE_OK)
    // {
    //     esp_log.task_printf("aqi server -> esp32:\n");
    //     DynamicJsonDocument jsonBuffer(2048);
    //     String str = Zlib::ungzipToString(httpClient.getStream());
    //     esp_log.println(str);
    //     deserializeJson(jsonBuffer, str);
    //     JsonObject root = jsonBuffer.as<JsonObject>();
    //     const char *category = root["now"]["category"];
    //     const char *aqi = root["now"]["aqi"];
    //     if (category != NULL && aqi != NULL)
    //     {
    //         sprintf(aqi_final, "%s", aqi);
    //         sprintf(category_final, "%s", category);
    //     }
    //     esp_log.println("url2 get");
    // }
    // else
    // {
    //     esp_log.task_printf("aqi server -/> esp32:\n");
    //     esp_log.println(httpCode);
    // }
    // httpClient.end();
}
void esp32_Http_weather()
{
    HTTPClient httpClient;
    httpClient.begin(WEATHER_URL);
    httpClient.setUserAgent(HTTP_USERAGENT);
    httpClient.addHeader("charset", "utf-8");
    int httpCode = http_get(httpClient);
    esp_log.task_printf("esp32 -> weather server:\n");
    if (httpCode == HTTP_CODE_OK)
    {
        esp_log.task_printf("weather server -> esp32:\n");
        DynamicJsonDocument jsonBuffer(2048);
        // String str = Zlib::ungzipToString(httpClient.getStream());
        // esp_log.println(str);
        deserializeJson(jsonBuffer, httpClient.getStream());
        JsonObject root = jsonBuffer.as<JsonObject>();
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
        esp_log.println(text);
    }
    else
    {
        esp_log.task_printf("weather server -/> esp32:\n");
        esp_log.println(httpCode);
    }
    httpClient.end();
}
void esp32_Http_hitokoto()
{
    HTTPClient httpClient2;
    httpClient2.begin("https://v1.hitokoto.cn/?encode=text&max_length=10");
    httpClient2.setUserAgent(HTTP_USERAGENT);
    httpClient2.addHeader("charset", "utf-8");
    int httpCode2 = http_get(httpClient2);
    esp_log.task_printf("esp32 -> hitokoto server\n");
    if (httpCode2 == HTTP_CODE_OK)
    {
        const String &payload2 = httpClient2.getString();
        esp_log.task_printf("hitokoto server -> esp32:\n");
        esp_log.println(payload2);
        if (payload2 != NULL)
        {
            sprintf(hitokoto_final, "%s", payload2.c_str());
        }
    }
    else
    {
        esp_log.task_printf("hitokoto server /> esp32:\n");
        esp_log.println(httpCode2);
    }
    httpClient2.end();
}
// 显示屏开关
void oled_show(const char *str1, const char *str2, const char *str3, const char *str4) // 提供三行英文输出,不保证异步安全
{
    if (oled_state == 1)
    {
        if (oled_mode == 1)
        {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_wqy16_t_gb2312);
            u8g2.setCursor(0, 13);
            u8g2.print(str1);
            u8g2.setCursor(0, 30);
            u8g2.print(str2);
            u8g2.setCursor(0, 47);
            u8g2.print(str3);
            u8g2.setFont(u8g2_font_wqy14_t_gb2312);
            u8g2.setCursor(0, 62);
            u8g2.print(str4);
            esp_log.printf("%s  %s  %s  %s -> oled\n", str1, str2, str3, str4);
            u8g2.sendBuffer();
        }
        else if (oled_mode == 2)
        {
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
            esp_log.println("oled_change");
            u8g2.sendBuffer();
            delay(700);
            oled_mode = 1; // 欢迎完正常
            esp_log.printf("欢迎回来 bszydxh  灯带就绪...  显示屏就绪... %s  -> oled\n", str4);
        }
        else if (oled_mode == 3)
        {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_wqy16_t_gb2312);
            u8g2.setCursor(0, 13);
            u8g2.print("!!!久坐提醒!!!");
            u8g2.setCursor(0, 30);
            char str_sit_time[100];
            char str_sit_start_time[100];
            sprintf(str_sit_start_time, "于%d时%d分坐下", time_hour, time_min);
            sprintf(str_sit_time, "您已久坐%d分钟", time_all);
            u8g2.print(str_sit_start_time);
            u8g2.setCursor(0, 47);
            u8g2.print(str_sit_time);
            u8g2.setFont(u8g2_font_wqy14_t_gb2312);
            u8g2.setCursor(0, 62);
            u8g2.print(str4);
            esp_log.println("oled_change");
            u8g2.sendBuffer();
            esp_log.printf("!!!久坐提醒!!!  %s  %s  %s  -> oled\n", str_sit_start_time, str_sit_time, str4);
            delay(10000);
            oled_mode = 1; // 提示完正常
        }
    }
    else if (oled_state == 0)
    {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        esp_log.printf("oled_off\n");
    }
}
int counter = 0;       // 官方计数
int rgb_screen_on = 0; // 逻辑上 rgb_screen ~= light_change 但light>rgb>mode
int rgb_frist_running = 0;
void button1_callback(const String &state)
{
    //./././
}
void rgb1_callback(uint8_t r_value, uint8_t g_value, uint8_t b_value, uint8_t bright_value)
{
    esp_log.task_printf("blinker -> rgb change\n");
    rgb_task_shutdown();
    light_on = 1; // 强制开启
    light_now = (r_value * 256 + g_value) * 256 + b_value;
    esp_log.println("color change");
    light_color_r[light_set.get_num()] = r_value;
    light_color_g[light_set.get_num()] = g_value;
    light_color_b[light_set.get_num()] = b_value;
    EEPROM_rgb_memory_commit();
    light_set.next();
    esp_log.printf("color:%d:%d:%d||%d\n", light_color_r[light_set.get_num()], light_color_g[light_set.get_num()], light_color_b[light_set.get_num()], light_set.get_num());
    on_sitclock();
    light_change = 1;
    mode = 3;
}
void BlinkerDataRead(const String &data)
{
    BLINKER_LOG("Blinker readString: ", data);

    Blinker.vibrate();

    uint32_t BlinkerTime = millis();

    Blinker.print("millis", BlinkerTime);
}
void rgbScreenTask(void *xTaskRgbScreen) // 流光溢彩任务
{
    uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;
    CRGB leds_rgb_mode[NUM_LEDS];
    while (1)
    {
        // 魔法包结构 Ada+校验码+rgb数据
        for (i = 0; i < sizeof(prefix); ++i) // 读到Ada开始
        {
        waitLoop:
            while (!Serial.available())
                ;
            // Check next byte in Magic Word
            if (prefix[i] == Serial.read())
                continue;
            i = 0;
            goto waitLoop;
        }
        while (!Serial.available())
            ;
        hi = Serial.read();
        while (!Serial.available())
            ;
        lo = Serial.read();
        while (!Serial.available())
            ;
        chk = Serial.read();
        // 检查校验码
        if (chk != (hi ^ lo ^ 0x55))
        {
            i = 0;
            goto waitLoop;
        } // 线程阻断

        for (uint8_t j = 0; j < NUM_LEDS; j++)
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
            leds_rgb_mode[j].r = r;
            leds_rgb_mode[j].g = g;
            leds_rgb_mode[j].b = b;
        }
        if (rgb_running == 1)
        {
            xQueueOverwrite(leds_queue, &leds_rgb_mode);
        }
        else
        {
            vTaskSuspend(NULL);
            delay(1000); // 留给调度器时间收回串口
        }
    }
}
void rgb_task_run()
{
    rgb_running = 1;
    if (rgb_frist_running == 0)
    {
        xTaskCreatePinnedToCore(rgbScreenTask, "Taskrgb", 4096, NULL, 2, &rgb_run, 1); // 只有第一次是创建任务
        rgb_frist_running = 1;
    }
    else
    {
        vTaskResume(rgb_run);
    }
}
void rgb_task_shutdown()
{
    rgb_running = 0;
    if (rgb_running == 1)
    {
        vTaskSuspend(rgb_run);
        delay(100);
    }
}
////////////////////////////////////////////////////////////////
void miotPowerState(const String &state)
{
    BLINKER_LOG("need set power state: ", state);

    if (state == BLINKER_CMD_ON)
    {
        light_on = 1;
        oled_state = 1;
        oled_mode = 2;
        esp_log.task_printf("miot -> light on\n");
        mode = 1; // 默认日光
        on_sitclock();
        light_change = 1;
        BlinkerMIOT.powerState("on");
        BlinkerMIOT.print();
    }
    else if (state == BLINKER_CMD_OFF)
    {
        light_set.reset();
        rgb_task_shutdown();
        mode = 0;
        esp_log.task_printf("miot -> light off\n");
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
    esp_log.task_printf("miot -> color change\n");
    rgb_task_shutdown();
    BLINKER_LOG("need set color: ", color);
    light_on = 1; // 强制开启
    light_now = color;
    uint8_t colorR = color >> 16 & 0xFF;
    uint8_t colorG = color >> 8 & 0xFF;
    uint8_t colorB = color & 0xFF;
    esp_log.printf("set color: %d\n", color);
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
    EEPROM_rgb_memory_commit();
    light_set.next();
    esp_log.printf("color:%d:%d:%d||%d\n", light_color_r[light_set.get_num()], light_color_g[light_set.get_num()], light_color_b[light_set.get_num()], light_set.get_num());
    on_sitclock();
    light_change = 1;
    mode = 3;
    BlinkerMIOT.color(color);
    BlinkerMIOT.print();
}
void miotBright(const String &bright)
{
    esp_log.task_printf("miot -> bright change\n");
    light_on = 1; // 强制开启
    mi_light_bright = bright.toInt();
    BLINKER_LOG("need set brightness: ", bright);
    light_brightness = bright.toInt() / 2 + bright.toInt() * 2;
    mode = 3;
    light_change = 1;
    on_sitclock();
    EEPROM_rgb_memory_commit();
    BLINKER_LOG("now set brightness: ", light_brightness);
    BlinkerMIOT.brightness(light_brightness);
    BlinkerMIOT.print();
    // oled_show("", "brightness", "change");
}
void miotMode(uint8_t mode_mi)
{
    BLINKER_LOG("need set mode: ", mode_mi);
    BlinkerMIOT.mode(mode_mi);
    BlinkerMIOT.print();
    mi_mode = mode_mi; // 用于回调的模式
    if (mode_mi == BLINKER_CMD_MIOT_DAY)
    {
        esp_log.task_printf("miot -> MIOT_DAY\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("color_off");                            // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        light_change = 1;
        mode = 3;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_NIGHT)
    {
        esp_log.task_printf("miot -> MIOT_NIGHT(回来了)\n");
        Udp.beginPacket("255.255.255.255", ESP32_KEYBOARD_PORT); // 配置远端ip地址和端口
        Udp.print("74245886off");                                // 把数据写入发送缓冲区
        Udp.endPacket();                                         // 发送数据
        miotPowerState("on");
        mi_mode = BLINKER_CMD_MIOT_DAY;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COLOR)
    {
        esp_log.task_printf("miot -> MIOT_COLOR\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("color_off");                            // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        light_color_r[0] = 255; // 通信量,上灯带
        light_color_g[0] = 0;   // 通信量
        light_color_b[0] = 0;   // 通信量
        light_color_r[1] = 0;   // 通信量,下灯带
        light_color_g[1] = 0;   // 通信量
        light_color_b[1] = 255; // 通信量
        light_color_r[2] = 255; // 通信量,侧灯带
        light_color_g[2] = 0;   // 通信量
        light_color_b[2] = 255; // 通信量
        EEPROM_rgb_memory_commit();
        // light_brightness = 255;
        mode = 3;
        light_change = 1;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_WARMTH)
    {
        esp_log.task_printf("miot -> MIOT_WARMTH(genshin)\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("genshin");                              // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        light_on = 1;
        oled_state = 1;
        rgb_screen_on = 1;
        light_change = 1;
        delay(100);
        esp_log.print("Ada\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("color");                                // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        on_sitclock();
    }
    else if (mode_mi == BLINKER_CMD_MIOT_TV)
    {
        esp_log.task_printf("miot -> MIOT_TV(turn off computer and light)\n");
        rgb_task_shutdown();
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("turn_off");                             // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        // oled_show("", "", "light off");
        esp_log.println("light off");
        off_sitclock();
        delay(4000);
        light_change = 1;
        mode = 0;
        oled_state = 0;
        light_on = 0;
        // 向udp工具发送消息
        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
    else if (mode_mi == BLINKER_CMD_MIOT_READING)
    {
        esp_log.task_printf("miot -> MIOT_READING\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("color_off");                            // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        light_color_r[0] = 255; // 通信量,上灯带
        light_color_g[0] = 255; // 通信量
        light_color_b[0] = 255; // 通信量
        light_color_r[1] = 255; // 通信量,下灯带
        light_color_g[1] = 150; // 通信量
        light_color_b[1] = 50;  // 通信量
        light_color_r[2] = 255; // 通信量,侧灯带
        light_color_g[2] = 150; // 通信量
        light_color_b[2] = 50;  // 通信量
        EEPROM_rgb_memory_commit();
        mode = 3;
        light_brightness = 200;
        light_change = 1;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COMPUTER)
    {

        esp_log.task_printf("miot -> MIOT_COMPUTER\n");
        light_on = 1;
        oled_state = 1;
        delay(300);
        rgb_screen_on = 1;
        light_change = 1;
        esp_log.print("Ada\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
        Udp.print("color");                                // 把数据写入发送缓冲区
        Udp.endPacket();                                   // 发送数据
        esp_log.println("UDP数据发送成功");
        on_sitclock();
    }
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
void oledTask(void *xTaskOled) // 显示屏任务
{
    while (1)
    {
        getLocalTime(&timeinfo, 100U);
        char str1[60];
        char str2[60];
        char str3[60];
        char str_timeinfo[60];
        char str_clockinfo[60];
        strftime(str_timeinfo, 100, "%a", &timeinfo);
        sprintf(str1, "%4d-%02d-%02d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, str_timeinfo); // 整合字符串
        strftime(str_clockinfo, 100, "%H:%M:%S", &timeinfo);
        if (rgb_running == 0)
        {
            if (WiFi.status() == WL_CONNECTED && Blinker.connected())
            {
                sprintf(str2, "%s 在线", str_clockinfo);
            }
            else
            {
                sprintf(str2, "%s 离线", str_clockinfo);
            }
        }
        else
        {
            sprintf(str2, "%s USB", str_clockinfo);
        }
        // sprintf(str2, "%s %s", str_clockinfo, text_final);
        if (timeinfo.tm_sec % 10 >= 5)
        {
            sprintf(str3, "%s|%s℃ %s", text_final, temp_final, aqi_final);
            oled_show(str1, str2, xd_final, hitokoto_final);
        }
        else
        {
            sprintf(str3, "%s|%s%% %s", text_final, humidity_final, category_final);
#if __has_include("xd.h")
            oled_show(str1, str2, xd_final, hitokoto_final);
#else
            oled_show(str1, str2, str3, hitokoto_final);
#endif
        }
        delay(300);
    }
}
void blinkerTask(void *xTaskBlinker) // blinker任务
{
    while (1)
    {
        Blinker.run();
        delay(100);
    }
}
void httpTask(void *xTaskHttp) // 巨型http请求模块任务,掌管http模块监控服务
{
    int system_state = NOT_SETUP;
    int system_state_last = NOT_SETUP;
    while (1)
    {
        delay(400);
        system_state_last = system_state;
        xQueuePeek(system_state_queue, &system_state, 100);
        if (system_state == NOT_SETUP) // http是服务端主体业务，由于blinker就一个黑盒，放在自己写的http服务下面
        {
            if (timeinfo.tm_year <= 100 || !strcmp(hitokoto_final, "松花酿酒,春水煎茶。"))
            {
                esp_log.println("configDNS&Time!");
                if (millis() > 100000)
                {
                    oled_show(DEVICE_NAME, "系统错误", "dns配置失败", "正在重启...");
                    WiFi.disconnect();
                    delay(300);
                    timeinfo.tm_year = 0;
                    esp_restart();
                }
                configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
                if (!getLocalTime(&timeinfo))
                {
                    continue;
                }
            }
            if (off_sitclock())
            {
                on_sitclock();
            }
            esp32_Http_weather();
            esp32_Http_aqi();
            esp32_Http_hitokoto(); // 获取一言
#if __has_include("xd.h")
            esp32_Http_XD();
#endif
            system_state = FINISH_SETUP;
            xQueueOverwrite(system_state_queue, &system_state);
        }
        if (system_state == FINISH_SETUP)
        {
            if (timeinfo.tm_sec == 10)
            {
                if (timeinfo.tm_min % 4 == 0)
                {
                    esp32_Http_weather();
                }
                if (timeinfo.tm_min % 4 == 1)
                {
                    esp32_Http_aqi();
                }
                if (timeinfo.tm_min % 4 == 2)
                {
                    esp32_Http_hitokoto();
                }
                if (timeinfo.tm_min % 4 == 3)
                {
#if __has_include("xd.h")
                    esp32_Http_XD();
#endif
                }
            }
        }
    }
}

void getStatePack(String &pack)
{
    char str1[60];
    char str2[60];
    char str3[60];
    char str_timeinfo[60];
    char str_clockinfo[60];
    strftime(str_timeinfo, 100, "%a", &timeinfo);
    sprintf(str1, "%4d-%02d-%02d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, str_timeinfo); // 整合字符串
    strftime(str_clockinfo, 100, "%H:%M:%S", &timeinfo);
    if (rgb_running == 0)
    {
        if (WiFi.status() == WL_CONNECTED && Blinker.connected())
        {
            sprintf(str2, "%s 在线", str_clockinfo);
        }
        else
        {
            sprintf(str2, "%s 离线", str_clockinfo);
        }
    }
    else
    {
        sprintf(str2, "%s USB", str_clockinfo);
    }
    if (timeinfo.tm_sec % 10 >= 5)
    {
        sprintf(str3, "%s|%s℃ %s", text_final, temp_final, aqi_final);
    }
    else
    {
        sprintf(str3, "%s|%s%% %s", text_final, humidity_final, category_final);
    }
    CRGB leds_temp[NUM_LEDS];
    if (!xQueuePeek(leds_queue, &leds_temp, 100) == pdTRUE)
    {
        memset(&leds_temp, 0, sizeof(CRGB[NUM_LEDS]));
    }
    DynamicJsonDocument response_dto(10240);
    response_dto["str1"] = str1;
    response_dto["str2"] = str2;
    response_dto["str3"] = str3;
    response_dto["str4"] = hitokoto_final;
    response_dto["state"] = light_on;
    response_dto["rgb"] = rgb_running;
    response_dto["light_mode"] = mode;
    response_dto["light_brightness"] = brightness_with_leds;
    JsonArray leds = response_dto.createNestedArray("leds");
    for (int i = 0; i < 120; i++)
    {
        uint32_t color = 0;
        color = leds_temp[i].b | leds_temp[i].g << 8 | leds_temp[i].r << 16;
        leds.add(color);
    }
    serializeJson(response_dto, pack);
}
void sendStateToAndroid(const char *host)
{
    Udp.beginPacket(host, ANDROID_PORT); // 配置远端ip地址和端口
    String response_dto = "";
    getStatePack(response_dto);
    Udp.print(response_dto);
    Udp.endPacket();
    esp_log.task_printf("state -> UDP\n");
}
void sendStateToPC(const char *host)
{
    // Udp.beginPacket(host, COMPUTER_PORT); // 配置远端ip地址和端口
    // String response_dto = "";
    // getStatePack(response_dto);
    // Udp.print(response_dto);
    // Udp.endPacket();
    // esp_log.task_printf("statePC -> UDP\n");
}
void udpTask(void *xTaskUdp)
{
    if (Udp.begin(ESP32_OLED_PORT))
    { // 启动Udp监听服务
        esp_log.println("监听成功");
        // 打印本地的ip地址，在UDP工具中会使用到
        //  WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
        esp_log.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), ESP32_OLED_PORT);
    }
    else
    {
        esp_log.error_printf("监听失败\n");
    }
    while (1)
    {
        delay(10);
        try
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                int packetSize = Udp.parsePacket(); // 获得解析包
                if (packetSize)                     // 解析包不为空
                {
                    esp_log.info_printf("Freeheap:%d\n", xPortGetFreeHeapSize());
                    esp_log.printf("%s:%d(%d)->UDP:\n", Udp.remoteIP().toString().c_str(), Udp.remotePort(), packetSize);
                    char incomingPacket[255];
                    int len = Udp.read(incomingPacket, 255); // 返回数据包字节数
                    if (len > 0)
                    {
                        esp_log.printf("%s\n", incomingPacket); // 向串口打印信息
                        incomingPacket[len] = 0;                // 清空缓存
                        if (strcmp(incomingPacket, "turn_on") == 0)
                        {
                            esp_log.task_printf("UDP -> miot\n");
                            miotPowerState("on");
                        }
                        else if (strcmp(incomingPacket, "turn_off") == 0)
                        {
                            esp_log.task_printf("UDP -> miot\n");
                            miotPowerState("off");
                        }
                        else if (strcmp(incomingPacket, "computer") == 0)
                        {
                            esp_log.task_printf("UDP -> miot\n");
                            miotMode(BLINKER_CMD_MIOT_COMPUTER);
                        }
                        else if (strcmp(incomingPacket, "normal_light") == 0)
                        {
                            esp_log.task_printf("UDP -> miot\n");
                            miotMode(BLINKER_CMD_MIOT_DAY);
                        }
                        else if (strcmp(incomingPacket, "computer?") == 0)
                        {
                            Udp.beginPacket(Udp.remoteIP().toString().c_str(), COMPUTER_PORT); // 配置远端ip地址和端口
                            if (rgb_running == 1)
                            {
                                Udp.print("computer->Y");
                            }
                            else
                            {
                                Udp.print("computer->N");
                            }
                            Udp.endPacket();
                        }
                        else if (strcmp(incomingPacket, "state") == 0)
                        {
                            esp_log.task_printf("UDP -> state\n");
                        }
                        sendStateToAndroid(Udp.remoteIP().toString().c_str());
                        sendStateToPC(Udp.remoteIP().toString().c_str());
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            esp_log.error_printf(e.what());
        }
    }
}
void tcpTask(void *xTaskTcp)
{
    WiFiServer TcpServer;
    WiFiClient TcpClients[MAX_SRV_CLIENTS];
    TcpServer.begin(ESP32_OLED_PORT);
    TcpServer.setNoDelay(true);
    esp_log.print(WiFi.localIP().toString());
    while (1)
    {
        int i = 0;
        // 有新连接
        if (TcpServer.hasClient())
        { // 判断是否有新的client请求进来
            for (i = 0; i < MAX_SRV_CLIENTS; i++)
            {

                // 释放旧无效或者断开的client
                if (!TcpClients[i] || !TcpClients[i].connected())
                {
                    if (!TcpClients[i])
                    {
                        // TcpClients[i]    判断指定序号的客户端是否有效
                        TcpClients[i].stop(); // 停止指定客户端的连接
                    }

                    TcpClients[i] = TcpServer.available(); // 分配最新的client
                    esp_log.print("1个新的客户端: ");
                    esp_log.println(i);
                    break; // 跳出一层for循环
                }
            }

            // 当达到最大连接数 无法释放无效的client，需要拒绝连接
            if (i == MAX_SRV_CLIENTS)
            {
                WiFiClient client = TcpServer.available();
                client.stop();
                Serial.println("连接被拒绝 ");
            }
        }

        // 检测client发过来的数据
        for (i = 0; i < MAX_SRV_CLIENTS; i++)
        {
            if (TcpClients[i] && TcpClients[i].connected())
            {
                if (TcpClients[i].available())
                {
                    String msg = TcpClients[i].readString();
                    esp_log.print(msg);
                    if (msg == "turn_on")
                    {
                        esp_log.task_printf("TCP -> miot\n");
                        miotPowerState("on");
                    }
                    else if (msg == "turn_off")
                    {
                        esp_log.task_printf("TCP -> miot\n");
                        miotPowerState("off");
                    }
                    else if (msg == "computer")
                    {
                        esp_log.task_printf("TCP -> miot\n");
                        miotMode(BLINKER_CMD_MIOT_COMPUTER);
                    }
                    else if (msg == "normal_light")
                    {
                        esp_log.task_printf("TCP -> miot\n");
                        miotMode(BLINKER_CMD_MIOT_DAY);
                    }
                    else if (msg == "computer?")
                    {
                        if (rgb_running == 1)
                        {
                            TcpClients[i].print("computer->Y");
                        }
                        else
                        {
                            TcpClients[i].print("computer->N");
                        }
                    }
                }
            }
        }
        delay(10);
    }
}
void udpConfigTask(void *xTaskConfigUdp)
{

    if (Udp.begin(ESP32_OLED_PORT))
    { // 启动Udp监听服务
        esp_log.println("监听成功");
        // 打印本地的ip地址，在UDP工具中会使用到
        //  WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
        esp_log.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), ESP32_OLED_PORT);
        sendStateToAndroid("255.255.255.255");
        sendStateToPC("255.255.255.255");
    }
    else
    {
        esp_log.error_printf("监听失败\n");
    }
    while (1)
    {
        delay(100);
        int packetSize = Udp.parsePacket(); // 获得解析包
        if (packetSize)                     // 解析包不为空
        {

            esp_log.printf("%s:%d(%d)->UDP:\n", Udp.remoteIP().toString().c_str(), Udp.remotePort(), packetSize);
            char incomingPacket[255];
            int len = Udp.read(incomingPacket, 255); // 返回数据包字节数
            String packet = incomingPacket;
            if (len > 0)
            {
                esp_log.printf("%s\n", incomingPacket); // 向串口打印信息
                incomingPacket[len] = 0;                // 清空缓存
                Udp.flush();
                if (strcmp(incomingPacket, "state") == 0)
                {
                    esp_log.task_printf("UDP -> state\n");
                    Udp.beginPacket(Udp.remoteIP().toString().c_str(), ANDROID_PORT); // 配置远端ip地址和端口
                    DynamicJsonDocument response_dto(10240);
                    char str_connect[100];
                    sprintf(str_connect, "%d台设备已连接", WiFi.softAPgetStationNum());
                    response_dto["str1"] = DEVICE_NAME;
                    response_dto["str2"] = "配网模式beta";
                    response_dto["str3"] = "启动app完成操作";
                    response_dto["str4"] = str_connect;
                    response_dto["state"] = -1;
                    response_dto["rgb"] = rgb_running;
                    response_dto["light_mode"] = -1;
                    response_dto["light_brightness"] = -1;
                    JsonArray leds = response_dto.createNestedArray("leds");
                    serializeJson(response_dto, Udp);
                    Udp.endPacket();
                    esp_log.task_printf("state -> UDP\n");
                }
                else if (packet.indexOf("{") != -1)
                {
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, packet);
                    String ssid = doc["ssid"];
                    String pwd = doc["pwd"];
                    if (!esp_EEPROM.begin(1024))
                    {
                        esp_log.println("eeprom fail!");
                        oled_show(DEVICE_NAME, "eeprom错误", "请等待", "正在重启...");
                        delay(100);
                        esp_restart();
                    } // 自定义从3000开始
                    oled_show(DEVICE_NAME, "已收到配网信息", "请等待", "正在重启...");
                    esp_EEPROM.writeString(100, ssid);
                    esp_EEPROM.writeString(200, pwd);
                    esp_EEPROM.commit();
                    esp_EEPROM.end();
                    delay(100);
                    esp_restart();
                }
            }
        }
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
    pinMode(button2_pin, INPUT);
    pinMode(button1_pin, INPUT);
    while (1)
    {
        button1_now = digitalRead(button1_pin);
        button2_now = digitalRead(button2_pin);
        if (button1_now == 0 && button1_before == 1)
        {
            esp_log.printf("\n1up\n");
        }
        else if (button1_now == 1 && button1_before == 0)
        {
            esp_log.printf("\n1down\n");
            if (light_on == 1)
            {
                esp_log.task_printf("button1 -> miot\n");
                miotPowerState("off");
            }
            else if (light_on == 0)
            {
                esp_log.task_printf("button1 -> miot\n");
                miotPowerState("on");
            }
        }
        if (button2_now == 0 && button2_before == 1)
        {
            esp_log.printf("\n2up\n");
        }
        else if (button2_now == 1 && button2_before == 0)
        {
            if (mi_mode == BLINKER_CMD_MIOT_DAY)
            {
                esp_log.task_printf("button2 -> miot\n");
                miotMode(BLINKER_CMD_MIOT_COMPUTER);
            }
            else if (mi_mode == BLINKER_CMD_MIOT_COMPUTER)
            {
                esp_log.task_printf("button2 -> miot\n");
                miotMode(BLINKER_CMD_MIOT_DAY);
            }
            else
            {
                esp_log.task_printf("button2 -> miot\n");
                miotMode(BLINKER_CMD_MIOT_DAY);
            }
            esp_log.printf("\n2down\n");
        }
        button1_before = button1_now;
        button2_before = button2_now;
        delay(10);
    }
}
void buttonConfigTask(void *xTaskButtonConfig)
{
    uint8_t button2_pin = 34;
    uint8_t button1_pin = 35;
    int button1_now = 0;
    int button2_now = 0;
    int button1_before = 0;
    int button2_before = 0;
    pinMode(button2_pin, INPUT);
    pinMode(button1_pin, INPUT);
    while (1)
    {
        button1_now = digitalRead(button1_pin);
        button2_now = digitalRead(button2_pin);
        if (button1_now == 0 && button1_before == 1)
        {
            esp_log.printf("\n1up\n");
        }
        else if (button1_now == 1 && button1_before == 0)
        {
            esp_log.printf("\n1down\n");
            int system_state = CONFIG_SETUP;
            xQueueOverwrite(system_state_queue, &system_state);
        }
        if (button2_now == 0 && button2_before == 1)
        {
            esp_log.printf("\n2up\n");
        }
        else if (button2_now == 1 && button2_before == 0)
        {
            esp_restart();
            esp_log.printf("\n2down\n");
        }
        button1_before = button1_now;
        button2_before = button2_now;
        delay(10);
    }
}
void fastledTask(void *xTaskfastled)
{
    CRGB show_leds[NUM_LEDS]; // 中间量
    CRGB last_leds[NUM_LEDS];
    int show_brightness = 255;
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(show_leds, NUM_LEDS);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (int i = 0; i < NUM_LEDS; i++)
    {
        CRGB empty;
        empty.r = 0;
        empty.g = 0;
        empty.b = 0;
        show_leds[i] = empty;
        last_leds[i] = empty;
    }
    while (1)
    {
        bool need_flashing = false;
        show_brightness = brightness_with_leds;
        if (xQueuePeek(leds_queue, &show_leds, 1) == pdTRUE)
        {
            if (rgb_running == 0)
            {
                for (int i = 0; i < NUM_LEDS; i++)
                {
                    if (last_leds[i] != show_leds[i])
                    {
                        last_leds[i] = show_leds[i];
                        need_flashing = true;
                    }
                }
            }
            else
            {
                need_flashing = true;
            }
            if (need_flashing)
            {
                FastLED.setBrightness(show_brightness);
                FastLED.show();
            }
        }
        vTaskDelayUntil(&xLastWakeTime, 5);
    }
}
void light_color_out(int *r, int *g, int *b, int bright)
{
    CRGB leds_temp[NUM_LEDS];
    CRGB light_change_color[3];
    int light_change_brightness = 0; // 中间变量
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
    if (!xQueuePeek(leds_queue, &leds_temp, 100) == pdTRUE)
    {
        memset(&leds_temp, 0, sizeof(CRGB[NUM_LEDS]));
    }

    for (int i = 0; i < 48; i++)
    {
        r_all[0] = leds_temp[i].r + r_all[0];
        g_all[0] = leds_temp[i].g + g_all[0];
        b_all[0] = leds_temp[i].b + b_all[0];
    }
    for (int i = 0; i < 48; i++)
    {
        r_all[1] = leds_temp[i + 48].r + r_all[1];
        g_all[1] = leds_temp[i + 48].g + g_all[1];
        b_all[1] = leds_temp[i + 48].b + b_all[1];
    }
    for (int i = 0; i < 24; i++)
    {
        r_all[2] = leds_temp[i + 96].r * 2 + r_all[2];
        g_all[2] = leds_temp[i + 96].g * 2 + g_all[2];
        b_all[2] = leds_temp[i + 96].b * 2 + b_all[2];
    }
    for (int i = 0; i < 3; i++)
    {
        light_now[i].r = r_all[i] / 48;
        light_now[i].g = g_all[i] / 48;
        light_now[i].b = b_all[i] / 48;
        esp_log.printf("light%d_all:%d:%d:%d\n", i, r_all[i], g_all[i], b_all[i]);
        esp_log.printf("light%d_now:%d:%d:%d\n", i, light_now[i].r, light_now[i].g, light_now[i].b);
    }
    for (int8_t n = 1; n <= 24; n++)
    {
        for (int i = 0; i < 3; i++)
        {
            light_change_color[i].r = r[i];
            light_change_color[i].g = g[i];
            light_change_color[i].b = b[i];
            light_change_brightness = light_now_brightness + (bright - light_now_brightness) * n / 24;
        }
        for (int i = 0; i < n; i++)
        {
            leds_temp[23 - i].r = light_change_color[0].r;
            leds_temp[23 - i].g = light_change_color[0].g;
            leds_temp[23 - i].b = light_change_color[0].b;
            leds_temp[24 + i].r = light_change_color[0].r;
            leds_temp[24 + i].g = light_change_color[0].g;
            leds_temp[24 + i].b = light_change_color[0].b;
            leds_temp[71 - i].r = light_change_color[1].r;
            leds_temp[71 - i].g = light_change_color[1].g;
            leds_temp[71 - i].b = light_change_color[1].b;
            leds_temp[72 + i].r = light_change_color[1].r;
            leds_temp[72 + i].g = light_change_color[1].g;
            leds_temp[72 + i].b = light_change_color[1].b;
            leds_temp[96 + i].r = light_change_color[2].r;
            leds_temp[96 + i].g = light_change_color[2].g;
            leds_temp[96 + i].b = light_change_color[2].b;
        }
        xQueueOverwrite(leds_queue, &leds_temp);
        brightness_with_leds = light_change_brightness;
        esp_log.printf("Light change\n");
        delay(5);
    }
    light_now_brightness = light_brightness;
}
void rgbChangeTask(void *xTaskRgbChange) // 灯条任务
{
    CRGB leds_temp[NUM_LEDS];
    while (1)
    {
        if (light_change == 1)
        {
            if (rgb_running == 0) // 1 全色
            {
                if (rgb_screen_on == 1)
                {
                    rgb_screen_on = 0;
                    rgb_task_run();
                    esp_log.println("light");
                    mode = 2;
                    light_change = 0;
                    continue;
                }
                if (mode == 1 || mode == 3) // 换亮度/色彩/////mode2悬空给rgb
                {
                    int r[3], g[3], b[3];
                    for (int i = 0; i < 3; i++)
                    {
                        r[i] = light_color_r[i];
                        g[i] = light_color_g[i];
                        b[i] = light_color_b[i];
                    }
                    light_color_out(r, g, b, light_brightness);
                    light_change = 0;
                    esp_log.println("led on");
                }
                else if (mode == 0) // 关led
                {
                    int r[3], g[3], b[3];
                    for (int i = 0; i < 3; i++)
                    {
                        r[i] = 0;
                        g[i] = 0;
                        b[i] = 0;
                    }
                    light_color_out(r, g, b, light_brightness);
                    light_change = 0;
                    esp_log.println("led off");
                }
                else if (mode == 4) // 久坐闪灯
                {
                    if (blink_time > 0)
                    {
                        int r1[3], g1[3], b1[3];
                        for (int i = 0; i < 3; i++)
                        {
                            r1[i] = 0;
                            g1[i] = 0;
                            b1[i] = 0;
                        }
                        light_color_out(r1, g1, b1, light_brightness);
                        esp_log.println("led blink");
                        delay(1000);
                        int r[3], g[3], b[3];
                        for (int i = 0; i < 3; i++)
                        {
                            r[i] = light_color_r[i];
                            g[i] = light_color_g[i];
                            b[i] = light_color_b[i];
                        }
                        light_color_out(r, g, b, light_brightness);
                        delay(1000);
                        light_change = 1;
                        esp_log.println("led blink");
                        blink_time--;
                    }
                    else
                    {
                        light_change = 0;
                        mode = 1;
                        esp_log.println("blink failed");
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
                    rgb_task_shutdown();
                    for (int i = 0; i < 120; i++)
                    {
                        leds_temp[i].r = 0;
                        leds_temp[i].g = 0;
                        leds_temp[i].b = 0;
                    }
                    xQueueOverwrite(leds_queue, &leds_temp);
                    delay(1000);
                    rgb_task_run();
                    delay(1000);
                    esp_log.println("led blink");
                    blink_time--;
                }
                else
                {
                    light_change = 0;
                    esp_log.println("blink failed");
                }
            }
        }
        delay(100);
    }
}
void debugTask(void *xTaskDebug) // debug...
{
    while (true)
    {
        esp_log.println("///////////////////////////////////\n");
        heap_caps_check_integrity_all(true);
        esp_log.printf("Freeheap:%d\n", xPortGetFreeHeapSize());
        esp_log.printf("FreeMinheap:%d\n", xPortGetMinimumEverFreeHeapSize());
        
        esp_log.printf("rgb:%d\n", uxTaskGetStackHighWaterMark(rgb_run));
        esp_log.printf("sit:%d\n", uxTaskGetStackHighWaterMark(sitclock_run));
        esp_log.printf("oled:%d\n", uxTaskGetStackHighWaterMark(oled_run));
        esp_log.printf("blinker:%d\n", uxTaskGetStackHighWaterMark(blinker_run));
        esp_log.printf("http:%d\n", uxTaskGetStackHighWaterMark(http_run));
        esp_log.printf("udp:%d\n", uxTaskGetStackHighWaterMark(udp_run));
        esp_log.printf("tcp:%d\n", uxTaskGetStackHighWaterMark(tcp_run));
        esp_log.printf("rgbc:%d\n", uxTaskGetStackHighWaterMark(rgbChange_run));
        esp_log.printf("button:%d\n", uxTaskGetStackHighWaterMark(button_run));
        esp_log.printf("fastled:%d\n", uxTaskGetStackHighWaterMark(fastled_run));
        esp_log.printf("debug:%d\n", uxTaskGetStackHighWaterMark(debug_run));
        esp_log.println("/////////////////////////////////////////////");
        delay(10000);
    }
}
void setup()
{
    leds_queue = xQueueCreate(1, sizeof(CRGB[NUM_LEDS])); // 创建led队列
    system_state_queue = xQueueCreate(1, sizeof(int));
    int system_state = NOT_SETUP;
    xQueueOverwrite(system_state_queue, &system_state);
    esp_log.setup();
    EEPROM_setup();
    const char *ssid = eeprom_ssid.c_str(); // 定义一个字符串(指针定义法)
    const char *password = eeprom_pwd.c_str();
    const char *auth = AUTH_KEY;
#ifdef DEBUG
    if (ESPLOG_LEVEL < ESPLOG_INFO)
    {
    }
    else
    {
        BLINKER_DEBUG.stream(Serial);
    }
    esp_log.set_log_out_level(ESPLOG_LEVEL);
#endif
    esp_log.println("bszydxh esp32 start!");
    u8g2.begin();
    u8g2.enableUTF8Print();
    oled_show(DEVICE_NAME, DEVICE_SUB_NAME, "搜索wifi中...", "初始化灯带...");
    WiFi.mode(WIFI_STA);
    IPAddress primaryDNS(114, 114, 114, 114); // 设置主DNS服务器
    IPAddress secondaryDNS(19, 29, 29, 29);   // 设置备用DNS服务器
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), primaryDNS, secondaryDNS);
    WiFi.begin(ssid, password);
    // WiFi.begin(SSID, PASSWORD);
    xTaskCreatePinnedToCore(buttonConfigTask, "buttonConfigTask", 4096, NULL, 2, &button_config_run, 0);
    while (WiFi.status() != WL_CONNECTED) // 线程阻断,等待网络连接
    {
        delay(1000);
        xQueuePeek(system_state_queue, &system_state, 100);
        if (system_state == CONFIG_SETUP)
        {
            break;
        }
        retry++;
        esp_log.println("no wifi!");
        esp_log.println(eeprom_ssid.c_str());
        esp_log.println(eeprom_pwd.c_str());
        char retry_str[50] = "0";
        sprintf(retry_str, "扫描次数: %d", retry);
        if (retry % 3 == 1)
        {
            oled_show(DEVICE_NAME, "wifi扫描中.", "如需配网按按钮一进入配网状态", retry_str);
        }
        if (retry % 3 == 2)
        {
            oled_show(DEVICE_NAME, "wifi扫描中..", "如需配网按按钮一进入配网状态", retry_str);
        }
        if (retry % 3 == 0)
        {
            oled_show(DEVICE_NAME, "wifi扫描中...", "如需配网按按钮一进入配网状态", retry_str);
        }
        if (retry == 15)
        {
            esp_log.error_printf("no wifi\n");
            char retry_time_str[10];
            for (int i = 5; i >= 0; i--)
            {
                sprintf(retry_time_str, "%d秒后自动重启", i);
                oled_show(DEVICE_NAME, "系统错误", "如需配网按按钮一进入配网状态", retry_time_str);
                delay(1000);
            }
            oled_show(DEVICE_NAME, "系统错误", "wifi寄了", "正在重启...");
            esp_restart();
        }
        WiFi.reconnect();
    }
    retry = 0;
    xQueuePeek(system_state_queue, &system_state, 100);
    if (system_state == CONFIG_SETUP)
    {
        esp_log.printf("enter config mode!\n");
        oled_show(DEVICE_NAME, "正在启动设备热点", "进入配网模式", "请稍后。。");
        WiFi.disconnect();
        WiFi.softAP("bszydxh_smart_light");
        WiFi.softAPsetHostname("bszydxh_smart_light");
        xTaskCreatePinnedToCore(udpConfigTask, "udpConfigTask", 7168, NULL, 0, &udp_config_run, 0);
        char str_connect[100];
        int time = 0;
        while (system_state == CONFIG_SETUP)
        {
            sprintf(str_connect, "%d台设备已连接", WiFi.softAPgetStationNum());
            if (time % 2 == 0)
            {
                oled_show("配网模式", "请连接设备wifi", "启动app进行配网", "退出请按按钮二");
            }
            else
            {
                oled_show("配网模式", "请连接设备wifi", "启动app进行配网", str_connect);
            }
            time++;
            delay(3000);
        }
        vTaskDelete(udp_config_run);
    }
    oled_show(DEVICE_NAME, DEVICE_SUB_NAME, "连接成功", "加载系统中...");
    Blinker.begin(auth, ssid, password);
    // Blinker.begin(auth, SSID, PASSWORD);
    vTaskDelete(button_config_run);
    Blinker.attachData(BlinkerDataRead);
    Button1.attach(button1_callback);
    RGB1.attach(rgb1_callback);
    BlinkerMIOT.attachPowerState(miotPowerState);
    BlinkerMIOT.attachColor(miotColor);
    BlinkerMIOT.attachMode(miotMode);
    BlinkerMIOT.attachBrightness(miotBright);
    BlinkerMIOT.attachQuery(miotQuery);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
    xTaskCreatePinnedToCore(oledTask, "oledTask", 4096, NULL, 2, &oled_run, 0);
    xTaskCreatePinnedToCore(blinkerTask, "blinkerTask", 48000, NULL, 2, &blinker_run, 0);
    xTaskCreatePinnedToCore(httpTask, "httpTask", 20480, NULL, 0, &http_run, 0);
    xTaskCreatePinnedToCore(udpTask, "udpTask", 7168, NULL, 2, &udp_run, 0);
    xTaskCreatePinnedToCore(tcpTask, "tcpTask", 7168, NULL, 2, &tcp_run, 0);
    xTaskCreatePinnedToCore(rgbChangeTask, "rgbChangeTask", 3072, NULL, 3, &rgbChange_run, 1); // 请不要动,动了就寄-以最高优先级运行
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 4096, NULL, 2, &button_run, 0);
    xTaskCreatePinnedToCore(fastledTask, "fastledTask", 2048, NULL, 3, &fastled_run, 1);
    // xTaskCreatePinnedToCore(debugTask, "debugTask", 2048, NULL, 3, &debug_run, 0);
}
void loop()
{
    esp_log.printf("Freeheap:%d\n", xPortGetFreeHeapSize());
    delay(5000); // 踢看门狗,loop本质上也是freertos中的一个任务
}