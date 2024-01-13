#ifndef __CONTROL_H__
#define __CONTROL_H__



void auroraSitClockBlink(int blink_time);
void auroraPowerState(const String &state);
void auroraColor(int32_t color);
void auroraBright(const String &bright);
void auroraMode(uint8_t mode_aurora);

#endif //!__CONTROL_H__