#ifndef __RGB_H__
#define __RGB_H__

extern TaskHandle_t rgb_run;
void                rgbScreenTask(void *xTaskRgbScreen); // 流光溢彩任务
void                rgb_task_run();
void                rgb_task_shutdown();
#endif //!__RGB_H__
