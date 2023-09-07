#ifndef __BUTTON_H__
#define __BUTTON_H__

extern TaskHandle_t button_run;
extern TaskHandle_t button_config_run;

void buttonTask(void *xTaskButton);

void buttonConfigTask(void *xTaskButtonConfig);

#endif //!__BUTTON_H__
