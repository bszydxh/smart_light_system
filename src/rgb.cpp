#include "main.h"
#include "utils.h"
#include "led.h"
#include "rgb.h"

int8_t rgb_running = 0; // 信号量,控制流光溢彩
int rgb_screen_on = 0;  // 逻辑上 rgb_screen ~= light_change 但light>rgb>mode//信号量
int rgb_frist_running = 0;
TaskHandle_t rgb_run;

void rgbScreenTask(void *xTaskRgbScreen) // 流光溢彩任务
{
  uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;
  LedState leds_rgb_mode_state;
  CRGB *leds_rgb_mode = leds_rgb_mode_state.leds;
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
      leds_rgb_mode[j].r = Serial.read();
      while (!Serial.available())
        ;
      leds_rgb_mode[j].g = Serial.read();
      while (!Serial.available())
        ;
      leds_rgb_mode[j].b = Serial.read();
    }
    if (rgb_running == 1)
    {
      xQueueOverwrite(leds_queue, &leds_rgb_mode_state);
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
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  rgb_running = 1;
  if (rgb_frist_running == 0)
  {
    xTaskCreatePinnedToCore(rgbScreenTask,
                            "Taskrgb",
                            4096,
                            NULL,
                            2,
                            &rgb_run,
                            1); // 只有第一次是创建任务
    rgb_frist_running = 1;
  }
  else
  {
    vTaskResume(rgb_run);
  }
  context.rgb_running = rgb_running;
  set_context(context);
}
void rgb_task_shutdown()
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  rgb_running = 0;
  if (rgb_running == 1)
  {
    vTaskSuspend(rgb_run);
    delay(100);
  }
  context.rgb_running = rgb_running;
  set_context(context);
}