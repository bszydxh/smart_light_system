#include "main.h"
#include "utils.h"
#include "data.h"
#include "sitclock.h"

TaskHandle_t sitclock_run;
// 久坐提醒部分,默认1h

// 算是手写看门狗...咬自己...
int  blink_time  = 5; // 闪灯次数
int  target_hour = -1;
int  target_min  = -1;
int  sitclock_on = 0; // 状态指示
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
  time_hour  = timeinfo.tm_hour;
  time_min   = timeinfo.tm_min;
  time_all   = 50;
  esp_log.printf("set_clock:%d:%d\n", target_hour, target_min);
}
void reset_sitclock_limit()
{
  target_hour = timeinfo.tm_hour;
  target_min  = timeinfo.tm_min + 10;
  time_all    = time_all + 10;
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
  target_min  = -1;
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
void on_sitclock() // 跟开灯绑定(含类似行为)
{
  if (sitclock_on == 0)
  {
    reset_sitclock();
    xTaskCreatePinnedToCore(sitclock_task,
                            "setclockTask",
                            2048,
                            NULL,
                            0,
                            &sitclock_run,
                            0);
  }
  sitclock_on = 1;
  esp_log.println("sitclock start!");
}
void sitClockBlink(int blink_time)
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  if (context.rgb_running == 0)
  {
    context.mode      = 4;
    context.oled_mode = 3;
  }
  else if (context.rgb_running == 1)
  {
    context.mode      = 5;
    context.oled_mode = 3;
  }
  context.blink_time = blink_time;
  set_context(context);
  xSemaphoreGive(led_semaphore);
}
void sitclock_task(void *sitclock_task_pointer)
{
  while (1)
  {
    esp_log.info_printf("sit clock try \n");
    // if (is_sitclock() == 1 && context.rgb_running == 0)
    // {
    //   esp_log.info_printf("sit clock warning!!!\n");
    //   // 久坐之后的操作
    //   mode         = 4;
    //   light_change = 1;
    //   oled_mode    = 3;
    //   blink_time   = (time_all - 40) / 10; // 动态增加闪灯次数
    //   reset_sitclock_limit();
    // }
    // else if (is_sitclock() == 1 && context.rgb_running == 1)
    // {
    //   esp_log.info_printf("sit clock warning!!!\n");
    //   // 久坐之后的操作
    //   mode         = 5;
    //   light_change = 1;
    //   oled_mode    = 3;
    //   blink_time   = (time_all - 40) / 10;
    //   reset_sitclock_limit();
    // }
    if (is_sitclock() == 1)
    {
      esp_log.info_printf("sit clock warning!!!\n");
      // 久坐之后的操作
      blink_time = (time_all - 40) / 10; // 动态增加闪灯次数
      sitClockBlink(blink_time);
      reset_sitclock_limit();
    }
    delay(5000);
  }
}