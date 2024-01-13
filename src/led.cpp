#include "main.h"
#include "utils.h"
#include "led.h"
QueueHandle_t leds_queue;

LightSet light_set;
// 掌管灯的过渡变化
void light_color_out(int *r, int *g, int *b, int bright)
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  LedState leds_temp_state;
  CRGB     light_change_color[3];
  int      light_change_brightness = 0; // 中间变量
  CRGB     light_now[3];                // 事件发生前的灯颜色
  int      r_all[3];
  int      g_all[3];
  int      b_all[3];
  for (int i = 0; i < 3; i++)
  {
    r_all[i] = 0;
    g_all[i] = 0;
    b_all[i] = 0;
  }
  if (!xQueuePeek(leds_queue, &leds_temp_state, 100) == pdTRUE)
  {
    memset(&leds_temp_state, 0, sizeof(LedState));
  }
  CRGB *leds_temp = leds_temp_state.leds;

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
    esp_log.printf("light%d_now:%d:%d:%d\n",
                   i,
                   light_now[i].r,
                   light_now[i].g,
                   light_now[i].b);
  } // 算平均值，过渡用
  for (int8_t n = 1; n <= 24; n++)
  {
    for (int i = 0; i < 3; i++)
    {
      light_change_color[i].r = r[i];
      light_change_color[i].g = g[i];
      light_change_color[i].b = b[i];
      light_change_brightness =
          context.brightness + (bright - context.brightness) * n / 24;
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
    leds_temp_state.brightness = light_change_brightness;
    xQueueOverwrite(leds_queue, &leds_temp_state);
    esp_log.printf("Light change\n");
    delay(5);
  }
}

void fastledTask(void *xTaskfastled)
{
  LedState show_leds_state;                  // 中间量
  CRGB    *show_leds = show_leds_state.leds; // 中间量
  CRGB     last_leds[NUM_LEDS];
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(show_leds, NUM_LEDS);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (int i = 0; i < NUM_LEDS; i++)
  {
    CRGB empty;
    empty.r      = 0;
    empty.g      = 0;
    empty.b      = 0;
    show_leds[i] = empty;
    last_leds[i] = empty;
  }
  while (1)
  {
    bool need_flashing = true;
    if (xQueuePeek(leds_queue, &show_leds_state, 1) == pdTRUE)
    {
      CRGB *show_leds = show_leds_state.leds; // 中间量
      // if (rgb_running == 0)
      // {
      //   for (int i = 0; i < NUM_LEDS; i++)
      //   {
      //     if (last_leds[i] != show_leds[i])
      //     {
      //       last_leds[i]  = show_leds[i];
      //       need_flashing = true;
      //     }
      //   }
      // }
      // else
      // {
      //   need_flashing = true;
      // }
      if (need_flashing)
      {
        FastLED.setBrightness(show_leds_state.brightness);
        FastLED.show();
      }
    }
    vTaskDelayUntil(&xLastWakeTime, 5);
  }
}