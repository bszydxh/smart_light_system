#ifndef __BLINKER_ESP32__
#define __BLINKER_ESP32__
extern TaskHandle_t blinker_run;

void miotPowerState(const String &state);
void miotColor(int32_t color);
void miotBright(const String &bright);
void miotMode(uint8_t mode_mi);
void blinkerTask(void *xTaskBlinker); // blinker任务

#endif //!__BLINKER_ESP32__