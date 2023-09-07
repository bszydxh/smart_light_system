#include "main.h"
#include "utils.h"
#include "blinker_esp32.h"
#include "control.h"
#ifdef USE_BLINKER
  #include "Blinker.h"
#else
  #include "ArduinoJson.h"
#endif

// blinker注册
#ifdef USE_BLINKER
char          b_name[32] = "btn-abc";
char          n_name[32] = "num-abc";
char          r_name[32] = "col-6ok";
BlinkerButton Button1(b_name);
BlinkerNumber Number1(n_name);
BlinkerRGB    RGB1(r_name);
TaskHandle_t  blinker_run;

void BlinkerDataRead(const String &data)
{
  BLINKER_LOG("Blinker readString: ", data);

  Blinker.vibrate();

  uint32_t BlinkerTime = millis();

  Blinker.print("millis", BlinkerTime);
}
#endif
////////////////////////////////////////////////////////////////
void miotPowerState(const String &state)
{
#ifdef USE_BLINKER
  BLINKER_LOG("need set power state: ", state);
  BlinkerMIOT.powerState(state);
  BlinkerMIOT.print();
  esp_log.task_printf("miot -> aurora\n");
#endif
  auroraPowerState(state);
}
void miotColor(int32_t color)
{
#ifdef USE_BLINKER
  uint8_t colorR = color >> 16 & 0xFF;
  uint8_t colorG = color >> 8 & 0xFF;
  uint8_t colorB = color & 0xFF;
  BLINKER_LOG("need set color: ", color);
  BLINKER_LOG("colorR: ", colorR, ", colorG: ", colorG, ", colorB: ", colorB);
  BlinkerMIOT.color(color);
  BlinkerMIOT.print();
#endif
  auroraColor(color);
}
void miotBright(const String &bright)
{
#ifdef USE_BLINKER
  BLINKER_LOG("need set brightness: ", bright);
  BLINKER_LOG("now set brightness: ", bright.toInt() / 2 + bright.toInt() * 2);
  BlinkerMIOT.brightness(bright.toInt() / 2 + bright.toInt() * 2);
  BlinkerMIOT.print();
#endif
  auroraBright(bright);
}
void miotMode(uint8_t mode_mi)
{
#ifdef USE_BLINKER
  BLINKER_LOG("need set mode: ", mode_mi);
  BlinkerMIOT.mode(mode_mi);
  BlinkerMIOT.print();
#endif
  auroraMode(mode_mi);
}

void miotQuery(int32_t queryCode)
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
#ifdef USE_BLINKER
  BLINKER_LOG("MIOT Query codes: ", queryCode);
  // 小爱指定的颜色,用于回调,与逻辑耦合的不是那么深,默认天蓝色,具体读eeprom里面的
  int light_now =
      (context.color_r[0] * 256 + context.color_g[0]) * 256 + context.color_b[0];
  switch (queryCode)
  {
  case BLINKER_CMD_QUERY_ALL_NUMBER:
    BLINKER_LOG("MIOT Query All");
    BlinkerMIOT.powerState(context.light_on ? "on" : "off");
    BlinkerMIOT.color(light_now);
    BlinkerMIOT.mode(context.mi_mode);
    BlinkerMIOT.brightness(context.brightness * 100 / 256);
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_POWERSTATE_NUMBER:
    BLINKER_LOG("MIOT Query Power State");
    BlinkerMIOT.powerState(context.light_on ? "on" : "off");
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_COLOR_NUMBER:
    BLINKER_LOG("MIOT Query Color");
    BlinkerMIOT.color(light_now);
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_MODE_NUMBER:
    BLINKER_LOG("MIOT Query Mode");
    BlinkerMIOT.mode(context.mi_mode);
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_COLORTEMP_NUMBER:
    BLINKER_LOG("MIOT Query ColorTemperature");
    // BlinkerMIOT.colorTemp(1000);
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_BRIGHTNESS_NUMBER:
    BLINKER_LOG("MIOT Query Brightness");
    BlinkerMIOT.brightness(context.brightness * 100 / 256);
    BlinkerMIOT.print();
    break;
  default:
    BLINKER_LOG("MIOT Query All");
    BlinkerMIOT.powerState(context.light_on ? "on" : "off");
    BlinkerMIOT.color(light_now);
    BlinkerMIOT.mode(context.mi_mode);
    BlinkerMIOT.brightness(context.brightness * 100 / 256);
    BlinkerMIOT.print();
    break;
  }
#endif
}
void rgb1_callback(uint8_t r_value,
                   uint8_t g_value,
                   uint8_t b_value,
                   uint8_t bright_value)
{
  miotColor(r_value << 16 | g_value << 8 | b_value);
}
void blinkerTask(void *xTaskBlinker) // blinker任务
{
#ifdef USE_BLINKER
  AuthPack *authPack = (AuthPack *)xTaskBlinker;
  Blinker.begin(authPack->auth, authPack->ssid, authPack->password);
  Blinker.attachData(BlinkerDataRead);
  RGB1.attach(rgb1_callback);
  BlinkerMIOT.attachPowerState(miotPowerState);
  BlinkerMIOT.attachColor(miotColor);
  BlinkerMIOT.attachMode(miotMode);
  BlinkerMIOT.attachBrightness(miotBright);
  BlinkerMIOT.attachQuery(miotQuery);
  #ifdef DEBUG
    #if ESPLOG_LEVEL >= ESPLOG_INFO
  BLINKER_DEBUG.stream(Serial);
    #endif
  #endif
#endif

  while (1)
  {
#ifdef USE_BLINKER
    Blinker.run();
    // SysContext context;
    // if (!get_context(context))
    // {
    //   continue;
    // }
    // if (Blinker.connected())
    // {
    //   context.is_mqtt_connect = true;
    // }
    // else
    // {
    //   context.is_mqtt_connect = false;
    // }
#endif
    delay(100);
  }
}
