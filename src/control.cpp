#include "main.h"
#include "utils.h"
#include "control.h"
#include "data.h"
#include "utils.h"
#include "sitclock.h"
#include "rgb.h"
#include "led.h"

void auroraPowerState(const String &state)
{
  SysContext context;

  if (state == "on")
  {
    esp_log.task_printf("aurora -> light on\n");
    if (!get_context(context))
    {
      return;
    }
    context.light_on   = 1;
    context.oled_state = 1;
    context.oled_mode  = 2;
    context.mode       = 1; // 默认日光
    set_context(context);
    xSemaphoreGive(led_semaphore);
    on_sitclock();
  }
  else if (state == "off")
  {
    esp_log.task_printf("aurora -> light off\n");
    rgb_task_shutdown();
    light_set.reset();
    if (!get_context(context))
    {
      return;
    }
    context.mode       = 0;
    context.oled_state = 0;
    context.light_on   = 0;
    set_context(context);
    xSemaphoreGive(led_semaphore);
    off_sitclock();
  }
}
void auroraColor(int32_t color)
{
  SysContext context;
  rgb_task_shutdown();
  if (!get_context(context))
  {
    return;
  }

  context.light_on = 1; // 强制开启
  uint8_t colorR   = color >> 16 & 0xFF;
  uint8_t colorG   = color >> 8 & 0xFF;
  uint8_t colorB   = color & 0xFF;
  esp_log.task_printf("miot -> color change:%d:::%d:%d:%d\n",
                      light_set.get_num(),
                      colorR,
                      colorG,
                      colorB);

  context.color_r[light_set.get_num()] = colorR;
  context.color_g[light_set.get_num()] = colorG;
  context.color_b[light_set.get_num()] = colorB;
  if (context.color_r[light_set.get_num()] == 255 &&
      context.color_g[light_set.get_num()] == 255 &&
      context.color_b[light_set.get_num()] == 0)
  {
    context.color_r[light_set.get_num()] = 255;
    context.color_g[light_set.get_num()] = 150;
    context.color_b[light_set.get_num()] = 50;
  }
  esp_log.printf("color:%d:%d:%d||%d\n",
                 context.color_r[light_set.get_num()],
                 context.color_g[light_set.get_num()],
                 context.color_b[light_set.get_num()],
                 light_set.get_num());
  light_set.next();
  on_sitclock();
  context.mode = 3;
  set_context(context);
  xSemaphoreGive(led_semaphore);
  EEPROM_rgb_memory_commit();
}
void auroraBright(const String &bright)
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  esp_log.task_printf("miot -> bright change\n");
  context.light_on   = 1; // 强制开启
  context.brightness = bright.toInt() / 2 + bright.toInt() * 2;
  context.mode       = 3;
  on_sitclock();
  set_context(context);
  xSemaphoreGive(led_semaphore);
  EEPROM_rgb_memory_commit();
}
void auroraMode(uint8_t mode_aurora)
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  context.mi_mode = mode_aurora;
  set_context(context);

  if (mode_aurora == BLINKER_CMD_MIOT_DAY)
  {
    esp_log.task_printf("miot -> MIOT_DAY\n");

    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("color_off");                            // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据

    esp_log.println("UDP数据发送成功");

    rgb_task_shutdown();
    if (!get_context(context))
    {
      return;
    }
    context.light_on   = 1;
    context.oled_state = 1;
    context.mode       = 3;
    set_context(context);
    xSemaphoreGive(led_semaphore);
    on_sitclock();
  }
  else if (mode_aurora == BLINKER_CMD_MIOT_NIGHT)
  {
    esp_log.task_printf("miot -> MIOT_NIGHT(回来了)\n");
    Udp.beginPacket("255.255.255.255", ESP32_KEYBOARD_PORT); // 配置远端ip地址和端口
    Udp.print("74245886off"); // 把数据写入发送缓冲区
    Udp.endPacket();          // 发送数据
    auroraPowerState("on");
    context.mi_mode = BLINKER_CMD_MIOT_DAY;
    set_context(context);
  }
  else if (mode_aurora == BLINKER_CMD_MIOT_COLOR)
  {
    esp_log.task_printf("miot -> MIOT_COLOR\n");
    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("color_off");                            // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据
    esp_log.println("UDP数据发送成功");
    rgb_task_shutdown();
    context.light_on   = 1;
    context.oled_state = 1;
    on_sitclock();
    context.color_r[0] = 255; // 通信量,上灯带
    context.color_g[0] = 0;   // 通信量
    context.color_b[0] = 0;   // 通信量
    context.color_r[1] = 0;   // 通信量,下灯带
    context.color_g[1] = 0;   // 通信量
    context.color_b[1] = 255; // 通信量
    context.color_r[2] = 255; // 通信量,侧灯带
    context.color_g[2] = 0;   // 通信量
    context.color_b[2] = 255; // 通信量
    context.mode       = 3;
    set_context(context);
    EEPROM_rgb_memory_commit();
    xSemaphoreGive(led_semaphore);
    // light_brightness = 255;
  }
  else if (mode_aurora == BLINKER_CMD_MIOT_WARMTH)
  {
    esp_log.task_printf("miot -> MIOT_WARMTH(genshin)\n");
    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("genshin");                              // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据
    esp_log.println("UDP数据发送成功");
    context.light_on   = 1;
    context.oled_state = 1;
    xSemaphoreGive(rgb_semaphore);
    delay(100);
    esp_log.print("Ada\n");
    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("color");                                // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据
    esp_log.println("UDP数据发送成功");
    on_sitclock();
    set_context(context);
    xSemaphoreGive(led_semaphore);
  }
  else if (mode_aurora == BLINKER_CMD_MIOT_TV)
  {
    esp_log.task_printf("miot -> MIOT_TV(turn off computer and light)\n");
    rgb_task_shutdown();
    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("turn_off");                             // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据
    esp_log.println("UDP数据发送成功");
    esp_log.println("light off");
    off_sitclock();
    delay(4000);
    context.mode       = 0;
    context.oled_state = 0;
    context.light_on   = 0;
    set_context(context);
    xSemaphoreGive(led_semaphore);
    // #ifdef USE_BLINKER
    //     BlinkerMIOT.powerState("off");
    //     BlinkerMIOT.print();
    // #endif
  }
  else if (mode_aurora == BLINKER_CMD_MIOT_READING)
  {
    esp_log.task_printf("miot -> MIOT_READING\n");
    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("color_off");                            // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据
    esp_log.println("UDP数据发送成功");
    rgb_task_shutdown();
    context.light_on   = 1;
    context.oled_state = 1;
    on_sitclock();
    context.color_r[0] = 255; // 通信量,上灯带
    context.color_g[0] = 255; // 通信量
    context.color_b[0] = 255; // 通信量
    context.color_r[1] = 255; // 通信量,下灯带
    context.color_g[1] = 150; // 通信量
    context.color_b[1] = 50;  // 通信量
    context.color_r[2] = 255; // 通信量,侧灯带
    context.color_g[2] = 150; // 通信量
    context.color_b[2] = 50;  // 通信量
    context.mode       = 3;
    context.brightness = 200;
    set_context(context);
    EEPROM_rgb_memory_commit();
    xSemaphoreGive(led_semaphore);
  }
  else if (mode_aurora == BLINKER_CMD_MIOT_COMPUTER)
  {

    esp_log.task_printf("miot -> MIOT_COMPUTER\n");
    context.light_on   = 1;
    context.oled_state = 1;
    delay(300);
    set_context(context);
    xSemaphoreGive(rgb_semaphore);
    xSemaphoreGive(led_semaphore);
    esp_log.print("Ada\n");
    Udp.beginPacket("255.255.255.255", COMPUTER_PORT); // 配置远端ip地址和端口
    Udp.print("color");                                // 把数据写入发送缓冲区
    Udp.endPacket();                                   // 发送数据
    esp_log.println("UDP数据发送成功");
    on_sitclock();
  }
  set_context(context);
}
