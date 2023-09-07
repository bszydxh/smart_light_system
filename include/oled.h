#ifndef __OLED_H__
#define __OLED_H__

extern TaskHandle_t oled_run;

void oled_setup();

void oled_show(const char *str1, const char *str2, const char *str3, const char *str4);

void oledTask(void *xTaskOled); // 显示屏任务

#endif //!__OLED_H__
