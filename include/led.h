#ifndef __LED_H__
#define __LED_H__

#define FASTLED_ALL_PINS_HARDWARE_SPI // 强制规定fastled

class LightSet
{
  int light_set_num = 0; // 通信量
public:
  void reset() { light_set_num = 0; }
  void next()
  {
    light_set_num++;
    if (light_set_num == 3)
    {
      light_set_num = 0;
    }
  }
  int get_num() { return light_set_num; }
};
extern LightSet      light_set;
extern QueueHandle_t leds_queue;

void light_color_out(int *r, int *g, int *b, int bright);
void fastledTask(void *xTaskfastled);

#endif //!__LED_H__
