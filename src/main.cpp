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
*/
/*系统由freertos接管*/
/*使用Espressif 32 platfromio 版本为4.3.0
#define CONFIG_FREERTOS_USE_TRACE_FACILITY
#define CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
/*delay()就是vTaskdelay(),不信自己跳转看一下*/
// todo 利用对列对信号进行消费
#include <Arduino.h>                  //主依赖,具体依赖见依赖树
#define FASTLED_ALL_PINS_HARDWARE_SPI //强制规定fastled
#include "SPI.h"                      //U8g2.h依赖 Blinker.h依赖
#include "Wire.h"                     //U8g2.h依赖
#include "freertos/FreeRTOS.h"
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
#include "U8g2lib.h"
#include "esp_task_wdt.h" //下面是和风天气的api,api的key手动再申请罢,一天3000次
#define URL "https://devapi.qweather.com/v7/weather/now?location=xxx&key=xxx&gzip=n"
#define URL2 "https://devapi.qweather.com/v7/air/now?location=xxx&key=xxx&gzip=n"
#define SSID ""
#define PASSWORD ""
#define AUTH_KEY ""
#define COMPUTER_PORT 8082
#define ANDROID_PORT 8081
#define ESP32_OLED_PORT 1145
#define ESP32_KEYBOARD_PORT 8484
//定义五行后把下面 #include "password.cpp" 去掉
#include "function.cpp"
#include "password.cpp"
#include "esp_heap_caps.h"
#define DEBUG                   //调试模式
#define ESPLOG_LEVEL ESPLOG_ALL //调试等级
////////////////////////////////////////////////////////////////
//灯光初始化定义
#define NUM_LEDS 120
#define DATA_PIN 25
#define USE_MULTCORE 1
//临界互斥锁,保护leds资源
static portMUX_TYPE leds_mutex = portMUX_INITIALIZER_UNLOCKED;
volatile CRGB leds[NUM_LEDS];            //实时量
volatile CRGB leds_rgb_mode[NUM_LEDS];   //实时量
volatile int brightness_with_leds = 255; //实时量
TaskHandle_t rgb_run;
TaskHandle_t sitclock_run;
TaskHandle_t oled_run;
TaskHandle_t blinker_run;
TaskHandle_t http_run;
TaskHandle_t udp_run;
TaskHandle_t rgbChange_run;
TaskHandle_t button_run;
TaskHandle_t fastled_run;
TaskHandle_t bleKeyboard_run;
////////////////////////////////////////////////////////////////
//全局初始化
WiFiUDP Udp;
#define NOT_SETUP 111
int8_t start_setup = NOT_SETUP;
struct tm timeinfo;      //时间信息
int retry = 0;           //记录重试次数,全局变量
const char *ssid = SSID; //定义一个字符串(指针定义法)
const char *password = PASSWORD;
const char *auth = AUTH_KEY;

const char *ntpServer1 = "ntp.ntsc.ac.cn"; //时间服务器
const char *ntpServer2 = "cn.ntp.org.cn";  //时间服务器
const char *ntpServer3 = "ntp.aliyun.com"; //时间服务器

const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/13, /* data=*/14); //定义u8g2
// class U8G2_SSD1306_128X32_NONAME_F_HW_I2C : public U8G2 {
//   public: U8G2_SSD1306_128X32_NONAME_F_HW_I2C(const u8g2_cb_t *rotation, uint8_t reset = U8X8_PIN_NONE, uint8_t clock = U8X8_PIN_NONE, uint8_t data = U8X8_PIN_NONE) : U8G2() {
//     u8g2_Setup_ssd1306_i2c_128x32_univision_f(&u8g2, rotation, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
//     u8x8_SetPin_HW_I2C(getU8x8(), reset, clock, data);
//   }
// };
// U8G2_SSD1306_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/13, /* data=*/14); //定义u8g2
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
volatile int8_t oled_state = 1;                             //通信量,显示屏开关
volatile int8_t oled_mode = 1;                              //通信量,显示屏模式 1 正常2 欢迎 3 久坐
volatile int8_t mode = 0;                                   //通信量,灯光改变模式,并非小爱指定的模式,
volatile int8_t light_on = 0;                               //小爱指定的开关状态,用于回调,与逻辑耦合的不是那么深,默认关
volatile int8_t mi_mode = 0;                                //小爱指定的模式,用于回调,与逻辑耦合的不是那么深,默认日光
volatile int mi_light_bright = 100;                         //小爱指定的亮度,用于回调,与逻辑耦合的不是那么深,默认100
volatile int32_t light_now = (135 * 256 + 206) * 256 + 235; //小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
volatile int8_t light_change = 0;                           //信号量,控制灯光
volatile int8_t rgb_running = 0;                            //信号量,控制流光溢彩
volatile int light_brightness = 255;                        //通信量
volatile int light_now_brightness = 255;
volatile int light_color_r[3]; //通信量
volatile int light_color_g[3]; //通信量
volatile int light_color_b[3]; //通信量
volatile int time_hour = 0;    //通信量
volatile int time_min = 0;     //通信量
volatile int time_all = 0;     //通信量

void hard_restart() //硬重启
{
    esp_task_wdt_init(1, true);
    esp_task_wdt_add(NULL);
    while (true)
        ;
}

////////////////////////////////////////////////////////////////
LightSet light_set;
ESPLog esp_log;
////////////////////////////////////////////////////////////////
// http请求部分,查天气,get
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
    esp_log.println("set_clock");
    if (timeinfo.tm_hour == 23)
    {
        target_hour = 0;
    }
    else
    {
        target_hour = timeinfo.tm_hour + 1;
        // target_hour = timeinfo.tm_hour;
    } //增加一个小时
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
void off_sitclock() //跟关灯绑定
{
    if (sitclock_on != 0)
    {
        esp_log.println("sitclock off!");
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
        esp_log.printf("sit clock try ");
        if (is_sitclock() == 1 && rgb_running == 0)
        {
            esp_log.printf("sit clock warning!!!");
            //久坐之后的操作
            mode = 4;
            light_change = 1;
            oled_mode = 3;
            blink_time = (time_all - 40) / 10; //动态增加闪灯次数
            reset_sitclock_limit();
        }
        else if (is_sitclock() == 1 && rgb_running == 1)
        {
            esp_log.printf("sit clock warning!!!");
            //久坐之后的操作
            mode = 5;
            light_change = 1;
            oled_mode = 3;
            blink_time = (time_all - 40) / 10;
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
    esp_log.println("sitclock start!");
}
////////////////////////////////////////////////////////////////
// eeprom掉电保护部分
EEPROMClass esp_EEPROM("esp_EEPROM");
void EEPROM_rgb_commit()
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
        oled_show("smart_screen", "eeprom错误", "请等待", "正在重启...");
        delay(100);
        hard_restart();
    } //自定义从3000开始
    // esp_log.printf("eeprom rgb commit check!\nrgb:%d:%d:%d\n", esp_EEPROM.readInt(3000), esp_EEPROM.readInt(3004), esp_EEPROM.readInt(3008));
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
    esp_log.printf("rgb1:%d:%d:%d", light_color_r_index[0], light_color_g_index[0], light_color_b_index[0]);
    esp_log.printf("rgb2:%d:%d:%d", light_color_r_index[1], light_color_g_index[1], light_color_b_index[1]);
    esp_log.printf("rgb3:%d:%d:%d", light_color_r_index[2], light_color_g_index[2], light_color_b_index[2]);
    esp_log.printf("bright:%d", light_brightness_index);
    esp_EEPROM.end();
}
void EEPROM_setup()
{
    if (!esp_EEPROM.begin(1024))
    {
        esp_log.error_printf("eeprom fail!");
        oled_show("smart_screen", "eeprom错误", "请等待", "正在重启...");
        delay(100);
        esp_restart();
    } //自定义从3000开始
    for (int i = 0; i <= 36; i = i + 4)
    {
        if (esp_EEPROM.readInt(i) == -1)
        {
            esp_EEPROM.put(i, 255);
            break;
        }
    }
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
    esp_log.printf("rgb1:%d:%d:%d", light_color_r[0], light_color_g[0], light_color_b[0]);
    esp_log.printf("rgb2:%d:%d:%d", light_color_r[1], light_color_g[1], light_color_b[1]);
    esp_log.printf("rgb3:%d:%d:%d", light_color_r[2], light_color_g[2], light_color_b[2]);
    esp_log.printf("bright:%d", light_brightness);
    light_now = (light_color_r[0] * 256 + light_color_g[0]) * 256 + light_color_b[0]; //小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
}
////////////////////////////////////////////////////////////////
void esp32_Http_covid()
{
    //创建 HTTPClient 对象
    HTTPClient httpClient3;
    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient3.begin("https://lab.isaaclin.cn/nCoV/api/area?country=%E4%B8%AD%E5%9B%BD&province=%E9%99%95%E8%A5%BF%E7%9C%81&latest=true");
    httpClient3.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    esp_log.print("URL: ");
    esp_log.println("https://lab.isaaclin.cn/nCoV/api/area?country=%E4%B8%AD%E5%9B%BD&province=%E9%99%95%E8%A5%BF%E7%9C%81&latest=trueURL");
    httpClient3.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode3 = httpClient3.GET();
    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode3 == HTTP_CODE_OK)
    {
        // String responsePayload = httpClient3.getString();
        const String &payload3 = httpClient3.getString();
        esp_log.println("Server Response Payload:");
        esp_log.println(payload3);
        DynamicJsonDocument jsonBuffer3(4096);
        deserializeJson(jsonBuffer3, payload3);
        JsonObject root3 = jsonBuffer3.as<JsonObject>();
        // JsonArray now = root["now"];
        int covid = root3["results"][0]["cities"][0]["currentConfirmedCount"];

        if (covid != 0)
        {
            sprintf(covid_final, "%d", covid);
        }
        esp_log.println(covid);
    }
    else
    {
        esp_log.println("Server Respose Code:");
        esp_log.println(httpCode3);
    }
    //关闭与服务器连接
    httpClient3.end();
}
void esp32_Http_aqi()
{
    //创建 HTTPClient 对象
    HTTPClient httpClient;
    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient.begin(URL2);
    httpClient.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    httpClient.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode = httpClient.GET();
    esp_log.println("Send GET request to URL: ");
    esp_log.println(URL2);

    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode == HTTP_CODE_OK)
    {
        // String responsePayload = httpClient.getString();
        // const String &payload = httpClient.getString();
        esp_log.println("Server Response Payload:");
        // esp_log.println(payload);
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
        esp_log.println("url2 get");
    }
    else
    {
        esp_log.println("Server Respose Code:");
        esp_log.println(httpCode);
    }
    //关闭与服务器连接
    httpClient.end();
}
void esp32_Http_weather()
{
    //创建 HTTPClient 对象
    HTTPClient httpClient;

    //配置请求地址。此处也可以不使用端口号和PATH而单纯的
    httpClient.begin(URL);
    httpClient.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36");
    esp_log.println("URL: ");
    esp_log.println(URL);
    httpClient.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode = httpClient.GET();
    esp_log.println("Send GET request to URL: ");
    esp_log.println(URL);

    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode == HTTP_CODE_OK)
    {
        // String responsePayload = httpClient.getString();
        // const String &payload = httpClient.getString();
        esp_log.println("Server Response Payload:");
        // esp_log.println(payload);
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
        esp_log.println(text);
    }
    else
    {
        esp_log.println("Server Respose Code:");
        esp_log.println(httpCode);
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
    esp_log.println("https://v1.hitokoto.cn/?encode=text&max_length=10");
    httpClient2.addHeader("charset", "utf-8");
    //启动连接并发送HTTP请求
    int httpCode2 = httpClient2.GET();
    esp_log.println("Send GET request to URL: ");

    //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
    //如果服务器不响应OK则将服务器响应状态码通过串口输出
    if (httpCode2 == HTTP_CODE_OK)
    {
        // String responsePayload = httpClient.getString();
        const String &payload2 = httpClient2.getString();
        esp_log.println("Server Response Payload:");
        esp_log.println(payload2);
        if (payload2 != NULL)
        {
            sprintf(hitokoto_final, "%s", payload2.c_str());
        }
    }
    else
    {
        esp_log.println("Server Respose Code:");
        esp_log.println(httpCode2);
    }
    //关闭与服务器连接
    httpClient2.end();
}
//显示屏开关
void oled_show(const char *str1, const char *str2, const char *str3, const char *str4) //提供三行英文输出,不保证异步安全
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
            esp_log.info_printf("oled_showing:%s%s%s%s", str1, str2, str3, str4);
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
            delay(10000);
            oled_mode = 1; //提示完正常
        }
    }
    else if (oled_state == 0)
    {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        esp_log.info_printf("oled_off");
    }
}
void print_oled() //用户界面,必须循环,否则出事
{
    if (!(getLocalTime(&timeinfo))) //获取时间不成功(一次也没)...允悲
    {
        delay(1000);
        // oled_show("error:404", "pls wait", "retrying...", "");
        esp_log.println("error:no connect");
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
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
        retry++;
        if (retry == 7)
        {
            esp_log.println("error:no connect");
            oled_show("smart_screen", "系统错误", "网络寄了", "正在重启...");
            hard_restart();
        }
        return;
    }
    if (start_setup == NOT_SETUP)
    {
        start_setup = 1;
    }
    char str1[60];
    char str2[60];
    char str3[60];
    char str_timeinfo[60];
    char str_clockinfo[60];
    strftime(str_timeinfo, 100, "%a", &timeinfo);
    sprintf(str1, "%4d-%02d-%02d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, str_timeinfo); //整合字符串
    strftime(str_clockinfo, 100, "%H:%M:%S", &timeinfo);
    if (rgb_running == 0)
    {
        if (WiFi.status() == WL_CONNECTED)
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
    }
    else
    {
        sprintf(str3, "%s|%s%% %s", text_final, humidity_final, category_final);
    }

    oled_show(str1, str2, str3, hitokoto_final);
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
    esp_log.task_printf("blinker -> rgb change");
    rgb_task_shutdown();
    light_on = 1; //强制开启
    light_now = (r_value * 256 + g_value) * 256 + b_value;
    esp_log.println("color change");
    light_color_r[light_set.get_num()] = r_value;
    light_color_g[light_set.get_num()] = g_value;
    light_color_b[light_set.get_num()] = b_value;
    EEPROM_rgb_commit();
    light_set.next();
    esp_log.printf("color:%d:%d:%d||%d\n", light_color_r[light_set.get_num()], light_color_g[light_set.get_num()], light_color_b[light_set.get_num()], light_set.get_num());
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
void rgbScreenTask(void *xTaskRgbScreen) //流光溢彩任务
{
    uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;
    while (1)
    {
        //  Wait for first byte of Magic Word
        //魔法包结构 Ada+校验码+rgb数据
        for (i = 0; i < sizeof(prefix); ++i) //读到Ada开始
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
        if (rgb_running == 1)
        {
            portENTER_CRITICAL(&leds_mutex);
            for (uint8_t i = 0; i < NUM_LEDS; i++)
            {
                leds[i].r = leds_rgb_mode[i].r;
                leds[i].g = leds_rgb_mode[i].g;
                leds[i].b = leds_rgb_mode[i].b;
            }
            portEXIT_CRITICAL(&leds_mutex);
        }
        else
        {
            delay(1000); //留给调度器时间收回串口
        }
    }
}

void rgb_task_run()
{
    rgb_running = 1;
    if (rgb_frist_running == 0)
    {
        xTaskCreatePinnedToCore(rgbScreenTask, "Taskrgb", 4096, NULL, 2, &rgb_run, 1); //只有第一次是创建任务
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
        delay(100);
        vTaskSuspend(rgb_run);
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
        esp_log.task_printf("miot -> light on");
        mode = 1; //默认日光
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
        esp_log.task_printf("miot -> light off");
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
    esp_log.task_printf("miot -> color change");
    rgb_task_shutdown();
    BLINKER_LOG("need set color: ", color);
    light_on = 1; //强制开启
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
    EEPROM_rgb_commit();
    light_set.next();
    esp_log.printf("color:%d:%d:%d||%d\n", light_color_r[light_set.get_num()], light_color_g[light_set.get_num()], light_color_b[light_set.get_num()], light_set.get_num());
    on_sitclock();
    light_change = 1;
    mode = 3;
    BlinkerMIOT.color(color);
    BlinkerMIOT.print();
    // oled_show("", "color", "change");
}
void miotBright(const String &bright)
{
    esp_log.task_printf("miot -> bright change");
    light_on = 1; //强制开启
    mi_light_bright = bright.toInt();
    BLINKER_LOG("need set brightness: ", bright);
    light_brightness = bright.toInt() / 2 + bright.toInt() * 2;
    mode = 3;
    light_change = 1;
    on_sitclock();
    EEPROM_rgb_commit();
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
    mi_mode = mode_mi; //用于回调的模式
    if (mode_mi == BLINKER_CMD_MIOT_DAY)
    {
        esp_log.task_printf("miot -> MIOT_DAY");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("color_off");                            //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
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
        esp_log.task_printf("miot -> MIOT_NIGHT(回来了)");
        Udp.beginPacket("255.255.255.255", ESP32_KEYBOARD_PORT); //配置远端ip地址和端口
        Udp.print("74245886off");                                //把数据写入发送缓冲区
        Udp.endPacket();                                         //发送数据
        miotPowerState("on");
        mi_mode = BLINKER_CMD_MIOT_DAY;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COLOR)
    {
        esp_log.task_printf("miot -> MIOT_COLOR");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("color_off");                            //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
        esp_log.println("UDP数据发送成功");
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
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
        // light_brightness = 255;
        mode = 3;
        light_change = 1;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_WARMTH)
    {
        esp_log.task_printf("miot -> MIOT_WARMTH(genshin)");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("genshin");                              //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
        esp_log.println("UDP数据发送成功");
        light_on = 1;
        oled_state = 1;
        rgb_screen_on = 1;
        light_change = 1;
        delay(100);
        esp_log.print("Ada\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("color");                                //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
        esp_log.println("UDP数据发送成功");
        on_sitclock();
    }
    else if (mode_mi == BLINKER_CMD_MIOT_TV)
    {
        esp_log.task_printf("miot -> MIOT_TV(turn off computer and light)");
        rgb_task_shutdown();
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("turn_off");                             //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
        esp_log.println("UDP数据发送成功");
        // oled_show("", "", "light off");
        esp_log.println("light off");
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
        esp_log.task_printf("miot -> MIOT_READING");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("color_off");                            //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
        esp_log.println("UDP数据发送成功");
        rgb_task_shutdown();
        light_on = 1;
        oled_state = 1;
        on_sitclock();
        light_color_r[0] = 255; //通信量,上灯带
        light_color_g[0] = 255; //通信量
        light_color_b[0] = 255; //通信量
        light_color_r[1] = 255; //通信量,下灯带
        light_color_g[1] = 150; //通信量
        light_color_b[1] = 50;  //通信量
        light_color_r[2] = 255; //通信量,侧灯带
        light_color_g[2] = 150; //通信量
        light_color_b[2] = 50;  //通信量
        EEPROM_rgb_commit();
        mode = 3;
        light_brightness = 200;
        light_change = 1;
    }
    else if (mode_mi == BLINKER_CMD_MIOT_COMPUTER)
    {

        esp_log.task_printf("miot -> MIOT_COMPUTER");
        light_on = 1;
        oled_state = 1;
        delay(300);
        rgb_screen_on = 1;
        light_change = 1;
        esp_log.print("Ada\n");
        Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
        Udp.print("color");                                //把数据写入发送缓冲区
        Udp.endPacket();                                   //发送数据
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
        // esp_log.printf("blinkerTask\n");
        Blinker.run();
        delay(100);
    }
}
void httpTask(void *xTaskHttp) //巨型http请求模块任务
{
    while (1)
    {
        delay(200);
        if (start_setup == 1)
        {
            esp32_Http_weather();
            esp32_Http_aqi();
            esp32_Http_hitokoto(); //获取一言
            // esp32_Http_covid();
            start_setup = 0;
        }
        if (start_setup == 0)
        {
            if (timeinfo.tm_min % 3 == 0 && timeinfo.tm_sec == 0)
            {
                esp32_Http_weather();
            }
            if (timeinfo.tm_min % 3 == 1 && timeinfo.tm_sec == 0)
            {
                esp32_Http_aqi();
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
            esp_log.info_printf("收到来自远程IP：%s（远程端口：%d）的数据包字节数：%d\n", Udp.remoteIP().toString().c_str(), Udp.remotePort(), packetSize);
            char incomingPacket[255];
            // 读取Udp数据包并存放在incomingPacket
            int len = Udp.read(incomingPacket, 255); //返回数据包字节数
            if (len > 0)
            {
                incomingPacket[len] = 0; //清空缓存
                if (strcmp(incomingPacket, "turn_on") == 0)
                {
                    esp_log.task_printf("UDP -> miot");
                    miotPowerState("on");
                }
                else if (strcmp(incomingPacket, "turn_off") == 0)
                {
                    esp_log.task_printf("UDP -> miot");
                    miotPowerState("off");
                }
                else if (strcmp(incomingPacket, "computer") == 0)
                {
                    esp_log.task_printf("UDP -> miot");
                    miotMode(BLINKER_CMD_MIOT_COMPUTER);
                }
                else if (strcmp(incomingPacket, "normal_light") == 0)
                {
                    esp_log.task_printf("UDP -> miot");
                    miotMode(BLINKER_CMD_MIOT_DAY);
                }
                else if (strcmp(incomingPacket, "computer?") == 0)
                {
                    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); //配置远端ip地址和端口
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
                    esp_log.task_printf("UDP -> state");
                    Udp.beginPacket("255.255.255.255", ANDROID_PORT); //配置远端ip地址和端口
                    char str1[60];
                    char str2[60];
                    char str3[60];
                    char str_timeinfo[60];
                    char str_clockinfo[60];
                    strftime(str_timeinfo, 100, "%a", &timeinfo);
                    sprintf(str1, "%4d-%02d-%02d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, str_timeinfo); //整合字符串
                    strftime(str_clockinfo, 100, "%H:%M:%S", &timeinfo);
                    if (rgb_running == 0)
                    {
                        if (WiFi.status() == WL_CONNECTED)
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
                    Udp.println(str1);
                    Udp.println(str2);
                    Udp.println(str3);
                    Udp.println(hitokoto_final);
                    Udp.println(light_on ? "状态：开" : "状态：关");
                    for (int i = 0; i < 3; i++)
                    {
                        Udp.printf("%d号灯带:%d:%d:%d\n", i,
                                   light_color_r[i],
                                   light_color_g[i],
                                   light_color_b[i]);
                    }
                    Udp.printf("模式:%d\n", mi_mode);
                    Udp.printf("亮度:%d\n", mi_light_bright);
                    Udp.endPacket();
                    esp_log.task_printf("state -> UDP");
                }
                esp_log.printf("UDP数据包内容为: %s\n", incomingPacket); //向串口打印信息
            }
        }
        delay(100); //延时3秒
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
        // esp_log.printf("pin1:%d;", digitalRead(button1_pin));
        // esp_log.printf("pin2:%d;\n", digitalRead(button2_pin));
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
                esp_log.task_printf("button1 -> miot");
                miotPowerState("off");
            }
            else if (light_on == 0)
            {
                esp_log.task_printf("button1 -> miot");
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
                esp_log.task_printf("button2 -> miot");
                miotMode(BLINKER_CMD_MIOT_COMPUTER);
            }
            else if (mi_mode == BLINKER_CMD_MIOT_COMPUTER)
            {
                esp_log.task_printf("button2 -> miot");
                miotMode(BLINKER_CMD_MIOT_DAY);
            }
            else
            {
                esp_log.task_printf("button2 -> miot");
                miotMode(BLINKER_CMD_MIOT_DAY);
            }
            esp_log.printf("\n2down\n");
        }
        button1_before = button1_now;
        button2_before = button2_now;
        delay(10);
    }
}
void fastledTask(void *xTaskfastled)
{
    CRGB show_leds[NUM_LEDS]; //中间量
    int show_brightness = 255;
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(show_leds, NUM_LEDS);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        int flag = 0;

        portENTER_CRITICAL(&leds_mutex);
        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            if (show_leds[i].r != leds[i].r)
            {
                show_leds[i].r = leds[i].r;
                flag = 1;
            }
            if (show_leds[i].g != leds[i].g)
            {
                show_leds[i].g = leds[i].g;
                flag = 1;
            }
            if (show_leds[i].b != leds[i].b)
            {
                show_leds[i].b = leds[i].b;
                flag = 1;
            }
            if (show_brightness != light_brightness)
            {
                show_brightness = brightness_with_leds;
                flag = 1;
            }
        }
        portEXIT_CRITICAL(&leds_mutex);
        if (flag == 1)
        {
            FastLED.setBrightness(show_brightness);
            FastLED.show();
        }
        vTaskDelayUntil(&xLastWakeTime, 10);
    }
}
void light_color_out(int *r, int *g, int *b, int bright)
{
    CRGB light_change_color[3];
    int light_change_brightness = 0; //中间变量
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
        esp_log.printf("%d:%d:%d\n", light_now[i].r, light_now[i].g, light_now[i].b);
    }
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
        brightness_with_leds = light_change_brightness;
        portEXIT_CRITICAL(&leds_mutex);
        esp_log.info_printf("Light change");
        delay(5);
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
                    esp_log.println("light");
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
                    esp_log.println("led on");
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
                    esp_log.println("led on");
                }
                else if (mode == 4) //久坐闪灯
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
                    // vTaskSuspend(rgb_run);
                    rgb_task_shutdown();
                    portENTER_CRITICAL(&leds_mutex);
                    for (int i = 0; i < 120; i++)
                    {
                        leds[i].r = 0;
                        leds[i].g = 0;
                        leds[i].b = 0;
                    }
                    portEXIT_CRITICAL(&leds_mutex);
                    // FastLED.show();
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
        esp_log.printf("Freeheap:%d\n", xPortGetFreeHeapSize());
        esp_log.printf("FreeMinheap:%d\n", xPortGetMinimumEverFreeHeapSize());
        esp_log.println("///////////////////////////////////");
        esp_log.print("rgb");
        esp_log.print(uxTaskGetStackHighWaterMark(rgb_run));
        esp_log.print("sit");
        esp_log.print(uxTaskGetStackHighWaterMark(sitclock_run));
        esp_log.print("oled");
        esp_log.print(uxTaskGetStackHighWaterMark(oled_run));
        esp_log.print("blinker");
        esp_log.print(uxTaskGetStackHighWaterMark(blinker_run));
        esp_log.print("http");
        esp_log.print(uxTaskGetStackHighWaterMark(http_run));
        esp_log.print("udp");
        esp_log.print(uxTaskGetStackHighWaterMark(udp_run));
        esp_log.print("rgbc");
        esp_log.print(uxTaskGetStackHighWaterMark(rgbChange_run));
        esp_log.print("button");
        esp_log.print(uxTaskGetStackHighWaterMark(button_run));
        esp_log.print("fastled");
        esp_log.println(uxTaskGetStackHighWaterMark(fastled_run));
        esp_log.println("/////////////////////////////////////////////");
        delay(1000);
    }
}
void setup()
{
    start_setup = NOT_SETUP;
    esp_log.setup();
    esp_log.println("bszydxh esp32 start!");
    WiFi.mode(WIFI_STA);
    u8g2.begin();
    u8g2.enableUTF8Print();
    esp_log.println("bszydxh blekey start!");
    // WiFi.begin(ssid, password);
    Blinker.begin(auth, ssid, password);
    oled_show("smart_screen", "---bszydxh", "搜索wifi中...", "初始化灯带...");
    while (WiFi.status() != WL_CONNECTED) //线程阻断,等待网络连接
    {
        delay(1000);
        retry++;
        esp_log.println("no wifi!");
        esp_log.println(ssid);
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
            esp_log.error_printf("no wifi");
            oled_show("smart_screen", "系统错误", "wifi寄了", "正在重启...");
            // esp_restart();
            hard_restart();
        }
    }
    retry = 0;
    oled_show("smart_screen", "---bszydxh", "连接成功", "加载系统中...");
    esp_log.println("wifi! done");
    if (Udp.begin(ESP32_OLED_PORT))
    { //启动Udp监听服务
        esp_log.println("监听成功");
        //打印本地的ip地址，在UDP工具中会使用到
        // WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
        esp_log.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), ESP32_OLED_PORT);
    }
    else
    {
        esp_log.error_printf("监听失败");
    }
    EEPROM_setup();
    Blinker.attachData(dataRead);
    Button1.attach(button1_callback);
    RGB1.attach(rgb1_callback);
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

    BlinkerMIOT.attachPowerState(miotPowerState);
    BlinkerMIOT.attachColor(miotColor);
    BlinkerMIOT.attachMode(miotMode);
    BlinkerMIOT.attachBrightness(miotBright);
    BlinkerMIOT.attachQuery(miotQuery);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
    light_change = 1;
    xTaskCreatePinnedToCore(oledTask, "oledTask", 3072, NULL, 1, &oled_run, 0);
    xTaskCreatePinnedToCore(blinkerTask, "blinkerTask", 7168, NULL, 2, &blinker_run, 0);
    xTaskCreatePinnedToCore(httpTask, "httpTask", 7168, NULL, 0, &http_run, 0);
    xTaskCreatePinnedToCore(udpTask, "udpTask", 4096, NULL, 0, &udp_run, 0);
    xTaskCreatePinnedToCore(rgbChangeTask, "rgbChangeTask", 3072, NULL, 3, &rgbChange_run, 1); //请不要动,动了就寄
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 4096, NULL, 2, &button_run, 0);
    xTaskCreatePinnedToCore(fastledTask, "fastledTask", 2048, NULL, 2, &fastled_run, 1);
    // xTaskCreatePinnedToCore(debugTask, "debugTask", 2048, NULL, 4, NULL, 0);
}
void loop()
{
    delay(5000); //踢看门狗,loop本质上也是freertos中的一个任务
}