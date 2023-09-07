#ifndef __SITCLOCK_H__
#define __SITCLOCK_H__

extern TaskHandle_t sitclock_run;

void reset_sitclock(); // 重置看门钟
void reset_sitclock_limit();
void on_sitclock();  // 跟开灯绑定(含类似行为)
bool off_sitclock(); // 跟关灯绑定
int  is_sitclock();
void sitclock_task(void *sitclock_task_pointer);
#endif //!__SITCLOCK_H__
