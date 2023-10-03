#include "main.h"
#include "utils.h"
#include "data.h"
#include "oled.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C
u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/13, /* data=*/14); // 定义u8g2
TaskHandle_t oled_run;
// 显示屏开关

void oled_setup()
{
  u8g2.begin();
  u8g2.enableUTF8Print();
}
void oled_show(const char *str1,
               const char *str2,
               const char *str3,
               const char *str4) // 提供三行英文输出,不保证异步安全
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  if (context.oled_state == 1)
  {
    if (context.oled_mode == 1)
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
    else if (context.oled_mode == 2)
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
      esp_log.printf("欢迎回来 bszydxh  灯带就绪...  显示屏就绪... %s  -> oled\n",
                     str4);
      if (!get_context(context))
      {
        return;
      }
      context.oled_mode = 1; // 欢迎完正常
      set_context(context);
    }
    else if (context.oled_mode == 3)
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
      esp_log.printf("!!!久坐提醒!!!  %s  %s  %s  -> oled\n",
                     str_sit_start_time,
                     str_sit_time,
                     str4);
      delay(10000);
      if (!get_context(context))
      {
        return;
      }
      context.oled_mode = 1; // 提示完正常
      set_context(context);
    }
  }
  else if (context.oled_state == 0)
  {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    esp_log.printf("oled_off\n");
  }
}

void oledTask(void *xTaskOled) // 显示屏任务
{
  SysContext context;
  while (1)
  {
    if (!get_context(context))
    {
      continue;
    }
    getLocalTime(&timeinfo, 100U);
    char str1[60];
    char str2[60];
    char str3[60];
    char str_timeinfo[60];
    char str_clockinfo[60];
    strftime(str_timeinfo, 100, "%a", &timeinfo);
    sprintf(str1,
            "%4d-%02d-%02d %s",
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            str_timeinfo); // 整合字符串
    strftime(str_clockinfo, 100, "%H:%M:%S", &timeinfo);
    if (context.rgb_running == 0)
    {
      String online_state = "离线";
#ifdef USE_BLINKER
      online_state = WiFi.status() == WL_CONNECTED ? "在线" : "离线";
      if (WiFi.status() == WL_CONNECTED && context.is_mqtt_connect)
      {
        online_state = "在线+";
      }
#else
      online_state = WiFi.status() == WL_CONNECTED ? "在线" : "离线";
#endif
      sprintf(str2, "%s %s", str_clockinfo, online_state);
    }
    else
    {
      sprintf(str2, "%s USB", str_clockinfo);
    }
    // sprintf(str2, "%s %s", str_clockinfo, text_final);
    if (timeinfo.tm_sec % 10 >= 5)
    {
      sprintf(str3, "%s|%s℃ %s", text_final, temp_final, aqi_final);
      oled_show(str1, str2, str3, hitokoto_final);
    }
    else
    {
      sprintf(str3, "%s|%s%% %s", text_final, humidity_final, category_final);
      // #if __has_include("xd.h")
      //       oled_show(str1, str2, xd_final, hitokoto_final);
      // #else
      //       oled_show(str1, str2, str3, hitokoto_final);
      // #endif
      oled_show(str1, str2, str3, hitokoto_final);
    }
    delay(300);
  }
}
