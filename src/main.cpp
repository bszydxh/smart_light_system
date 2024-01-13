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
#include "main.h"
#include "utils.h"
#include "data.h"
#include "led.h"
#include "rgb.h"
#include "oled.h"
#include "sitclock.h"
#include "blinker_esp32.h"
#include "http.h"
#include "socket.h"
#include "button.h"
#include "socket.h"

#if __has_include("xd.h") // 非c++官方用法，xd.h是项目作者自己的魔改部分，看情况删
#include "xd.h"
#endif
struct tm timeinfo; // 定义时间信息
int retry = 0;      // 记录重试次数,全局变量
WiFiUDP Udp;

QueueHandle_t system_state_queue;
QueueHandle_t sys_context_queue;
SemaphoreHandle_t led_semaphore;
SemaphoreHandle_t rgb_semaphore;
SysContext context;
TaskHandle_t rgbChange_run;
TaskHandle_t fastled_run;
TaskHandle_t debug_run;

const char *ntpServer1 = "ntp.ntsc.ac.cn"; // 时间服务器1
const char *ntpServer2 = "cn.ntp.org.cn";  // 时间服务器2
const char *ntpServer3 = "ntp.aliyun.com"; // 时间服务器3
const long gmtOffset_sec = 8 * 3600;       // 时区
const int daylightOffset_sec = 0;          // 夏令时偏移

void rgbChangeTask(void *xTaskRgbChange) // 灯条任务
{
  SysContext context;

  CRGB leds_temp[NUM_LEDS];

  while (1)
  {
    delay(100);
    if (xSemaphoreTake(led_semaphore, 100) == pdTRUE)
    {
      if (!get_context(context))
      {
        continue;
      }
      delay(30);
      esp_log.printf("get_context|light_on:%d|mode:%d|oled_mode:%d|oled_state:%d\n",
                     context.light_on,
                     context.mode,
                     context.oled_mode,
                     context.oled_state);
      if (context.rgb_running == 0) // 1 全色
      {
        if (xSemaphoreTake(rgb_semaphore, 100) == pdTRUE)
        {
          context.mode = 2;
          set_context(context);
          rgb_task_run();
          esp_log.println("light");
          continue;
        }
        if (context.mode == 1 || context.mode == 3) // 换亮度/色彩/////mode2悬空给rgb
        {
          int r[3], g[3], b[3];
          for (int i = 0; i < 3; i++)
          {
            r[i] = context.color_r[i];
            g[i] = context.color_g[i];
            b[i] = context.color_b[i];
          }
          light_color_out(r, g, b, context.brightness);
          esp_log.println("led on");
        }
        else if (context.mode == 0) // 关led
        {
          int r[3], g[3], b[3];
          for (int i = 0; i < 3; i++)
          {
            r[i] = 0;
            g[i] = 0;
            b[i] = 0;
          }
          light_color_out(r, g, b, context.brightness);
          esp_log.println("led off");
        }
        else if (context.mode == 4) // 久坐闪灯
        {
          if (context.blink_time > 0)
          {
            int r1[3], g1[3], b1[3];
            for (int i = 0; i < 3; i++)
            {
              r1[i] = 0;
              g1[i] = 0;
              b1[i] = 0;
            }
            light_color_out(r1, g1, b1, context.brightness);
            esp_log.println("led blink");
            delay(1000);
            int r[3], g[3], b[3];
            for (int i = 0; i < 3; i++)
            {
              r[i] = context.color_r[i];
              g[i] = context.color_g[i];
              b[i] = context.color_b[i];
            }
            light_color_out(r, g, b, context.brightness);
            delay(1000);
            xSemaphoreGive(led_semaphore);
            esp_log.println("led blink");
            context.blink_time--;
            set_context(context);
          }
          else
          {
            context.mode = 1;
            esp_log.println("blink failed");
            set_context(context);
          }
        }
        else if (context.mode == 5)
        {
          context.mode = 2;
          set_context(context);
        }
      }
      else if (context.rgb_running == 1 && context.mode == 5) // usb模式久坐闪灯
      {
        if (context.blink_time > 0)
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
          context.blink_time--;
          set_context(context);
        }
        else
        {
          esp_log.println("blink failed");
        }
      }
    }
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
  WiFi.disconnect(true);
  leds_queue = xQueueCreate(1, sizeof(LedState)); // 创建led队列
  system_state_queue = xQueueCreate(1, sizeof(int));
  sys_context_queue = xQueueCreate(1, sizeof(SysContext));
  led_semaphore = xSemaphoreCreateBinary();
  rgb_semaphore = xSemaphoreCreateBinary();
  int system_state = NOT_SETUP;
  xQueueOverwrite(system_state_queue, &system_state);
  SysContext context;
  set_context(context);
  esp_log.setup();
  EEPROM_setup();
  const char *ssid = eeprom_ssid.c_str(); // 定义一个字符串(指针定义法)
  const char *password = eeprom_pwd.c_str();
  const char *auth = AUTH_KEY;
#ifdef DEBUG
  esp_log.set_log_out_level(ESPLOG_LEVEL);
#endif
  esp_log.println("bszydxh esp32 start!");
  oled_setup();
  oled_show(DEVICE_NAME, DEVICE_SUB_NAME, "搜索wifi中...", "初始化灯带...");
  WiFi.mode(WIFI_STA);
  IPAddress primaryDNS(114, 114, 114, 114); // 设置主DNS服务器
  IPAddress secondaryDNS(19, 29, 29, 29);   // 设置备用DNS服务器
  WiFi.config(IPAddress(0, 0, 0, 0),
              IPAddress(0, 0, 0, 0),
              IPAddress(0, 0, 0, 0),
              primaryDNS,
              secondaryDNS);

  WiFi.begin(ssid, password);
  // WiFi.begin(SSID, PASSWORD);
  xTaskCreatePinnedToCore(buttonConfigTask,
                          "buttonConfigTask",
                          4096,
                          NULL,
                          2,
                          &button_config_run,
                          0);
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
      oled_show(DEVICE_NAME,
                "wifi扫描中...",
                "如需配网按按钮一进入配网状态",
                retry_str);
    }
    if (retry == 15)
    {
      esp_log.error_printf("no wifi\n");
      char retry_time_str[10];
      for (int i = 5; i >= 0; i--)
      {
        sprintf(retry_time_str, "%d秒后自动重启", i);
        oled_show(DEVICE_NAME,
                  "系统错误",
                  "如需配网按按钮一进入配网状态",
                  retry_time_str);
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
    xTaskCreatePinnedToCore(udpConfigTask,
                            "udpConfigTask",
                            7168,
                            NULL,
                            0,
                            &udp_config_run,
                            0);
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
  vTaskDelete(button_config_run);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
  xTaskCreatePinnedToCore(oledTask, "oledTask", 4096, NULL, 2, &oled_run, 0);
#ifdef USE_BLINKER
  AuthPack authPack;
  authPack.auth = auth;
  authPack.ssid = ssid;
  authPack.password = password;
  xTaskCreatePinnedToCore(blinkerTask,
                          "blinkerTask",
                          4096,
                          (void *)&authPack,
                          2,
                          &blinker_run,
                          0);
#endif
  xTaskCreatePinnedToCore(httpTask, "httpTask", 10240, NULL, 0, &http_run, 0);
  xTaskCreatePinnedToCore(udpTask, "udpTask", 2048, NULL, 2, &udp_run, 0);
  xTaskCreatePinnedToCore(tcpTask, "tcpTask", 4096, NULL, 2, &tcp_run, 0);
  xTaskCreatePinnedToCore(buttonTask, "buttonTask", 4096, NULL, 2, &button_run, 0);
  xTaskCreatePinnedToCore(rgbChangeTask,
                          "rgbChangeTask",
                          3072,
                          NULL,
                          3,
                          &rgbChange_run,
                          1); // 请不要动,动了就寄-以最高优先级运行
  xTaskCreatePinnedToCore(fastledTask, "fastledTask", 2048, NULL, 3, &fastled_run, 1);
  // xTaskCreatePinnedToCore(debugTask, "debugTask", 2048, NULL, 3, &debug_run, 0);
}
void loop()
{
  esp_log.printf("Freeheap:%d\n", xPortGetFreeHeapSize());
  delay(5000); // 踢看门狗,loop本质上也是freertos中的一个任务
}
bool get_context(SysContext &Context)
{
  bool a = (xQueuePeek(sys_context_queue, &Context, 0) == pdTRUE);
  return a;
}
void set_context(SysContext &Context)
{
  esp_log.printf("set_context|light_on:%d|mode:%d|oled_mode:%d|oled_state:%d|rgb_running:%d\n",
                 Context.light_on,
                 Context.mode,
                 Context.oled_mode,
                 Context.oled_state,
                 Context.rgb_running);
  xQueueOverwrite(sys_context_queue, &Context);
}