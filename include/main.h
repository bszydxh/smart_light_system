#ifndef __MIAN_H__
#define __MIAN_H__

#include <Arduino.h> //主依赖,具体依赖见依赖树
#include "SPI.h"     //U8g2.h依赖 Blinker.h依赖
#include "Wire.h"    //U8g2.h依赖
#include "freertos/FreeRTOS.h"
#include "ESPmDNS.h"
#include "FS.h"
#include "SPIFFS.h"
#include "Ticker.h"
#include "Update.h"
#include "U8g2lib.h"

#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "EEPROM.h"
#include "HTTPClient.h"
#include "FastLED.h"
#include "AsyncUDP.h"

#include "esp_task_wdt.h"
#include "esp_heap_caps.h"

//////////////初始化定义/////////////////
#define CONFIG_SETUP 2
#define NOT_SETUP 1
#define FINISH_SETUP 0

//////////////配置/////////////////

#define USE_MULTCORE 1 // 使用双核
#define DEST_FS_USES_LITTLEFS

#define DEBUG                   // 调试模式
#define ESPLOG_LEVEL ESPLOG_ALL // 调试等级

// 灯光初始化定义
#define NUM_LEDS 120
#define DATA_PIN 25

//////////////socket相关//////////////
#define COMPUTER_PORT 8082
#define ANDROID_PORT 8081
#define ESP32_OLED_PORT 1145
#define ESP32_KEYBOARD_PORT 8484
#define MAX_SRV_CLIENTS 4

struct SysContext
{
  uint8_t light_on = 0; // 小爱指定的开关状态,用于回调,与逻辑耦合的不是那么深,默认关
  uint8_t mode = 0;     // 通信量,灯光改变模式,并非小爱指定的模式
  uint8_t rgb_running = 0;
  uint8_t blink_time = 0; // 剩余闪光次数
  uint8_t color_r[3] = {0xff};
  uint8_t color_g[3] = {0xff};
  uint8_t color_b[3] = {0xff};
  uint8_t brightness = 0xff;
  uint8_t oled_state = 1; // 通信量,显示屏开关
  uint8_t oled_mode = 1;  // 通信量,显示屏模式 1 正常2 欢迎 3 久坐
  uint8_t mi_mode = 0;    // 小爱指定的模式,用于回调,与逻辑耦合的不是那么深,默认日光
  bool is_mqtt_connect = false;
};

struct LedState
{
  CRGB leds[NUM_LEDS] = {0};
  int brightness = 255;
};

struct AuthPack
{
  const char *ssid;
  const char *password;
  const char *auth;
};

//////////////freertos相关//////////////

//////////////Blinker相关///////////////

//////////////初始化配置/////////////////
#if __has_include("config.cpp") // 非c++官方用法，config.cpp是项目作者自己的配置文件
#include "config.cpp"
#else
//!!!请配置以下内容，否则无法正常运行!!!
//天气改用心知天气https://seniverse.yuque.com/hyper_data/api_v3/nyiu3t
#define WEATHER_URL \
  "https://api.seniverse.com/v3/weather/now.json?key=your_api_key&location=beijing&language=zh-Hans&unit=c"
#define SSID_LOACL ""
#define PASSWORD_LOACL 0 // 也可以手动配置，就不用自动配网

#define AUTH_KEY ""
#define DEVICE_NAME "smart_light"
#define DEVICE_SUB_NAME "--bszydxh"
#endif

#define USE_BLINKER // 是否启用小爱同学扩展支持

#ifdef USE_BLINKER
#define BLINKER_PRINT Serial // Blinker.h依赖
#define BLINKER_WIFI         // Blinker.h依赖
#define BLINKER_MIOT_LIGHT
#define BLINKER_WITHOUT_SSL
#define BLINKER_NO_LOGO
#endif
extern struct tm timeinfo;           // 定义时间信息
extern const char *ntpServer1;       // 时间服务器1
extern const char *ntpServer2;       // 时间服务器2
extern const char *ntpServer3;       // 时间服务器3
extern const long gmtOffset_sec;     // 时区
extern const int daylightOffset_sec; // 夏令时偏移
extern WiFiUDP Udp;
extern QueueHandle_t system_state_queue;
extern QueueHandle_t sys_context_queue;
extern SemaphoreHandle_t led_semaphore;
extern SemaphoreHandle_t rgb_semaphore;
bool get_context(SysContext &Context);
void set_context(SysContext &Context);
#endif //!__MIAN_H__
