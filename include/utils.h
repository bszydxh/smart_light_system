#ifndef __UTILS_H__
#define __UTILS_H__

#define ESPLOG_ALL   6
#define ESPLOG_DEBUG 5
#define ESPLOG_INFO  4
#define ESPLOG_TASK  3
#define ESPLOG_WARN  2
#define ESPLOG_ERROR 1
#define ESPLOG_OFF   0

#define BLINKER_CMD_MIOT_DAY      0
#define BLINKER_CMD_MIOT_NIGHT    1
#define BLINKER_CMD_MIOT_COLOR    2
#define BLINKER_CMD_MIOT_WARMTH   3
#define BLINKER_CMD_MIOT_TV       4
#define BLINKER_CMD_MIOT_READING  5
#define BLINKER_CMD_MIOT_COMPUTER 6

#include <Arduino.h>
class ESPLog
{
  int log_level = 0;

public:
  void setup()
  {
    Serial.begin(256000);
    delay(500);
  }
  void set_log_out_level(int n) // 默认为warning
  {
    if (n == ESPLOG_ALL)
    {
      log_level = ESPLOG_ALL;
    }
    else if (n == ESPLOG_INFO)
    {
      log_level = ESPLOG_INFO;
    }
    else if (n == ESPLOG_TASK)
    {
      log_level = ESPLOG_TASK;
    }
    else if (n == ESPLOG_WARN)
    {
      log_level = ESPLOG_WARN;
    }
    else if (n == ESPLOG_ERROR)
    {
      log_level = ESPLOG_ERROR;
    }
    else if (n == ESPLOG_OFF)
    {
      log_level = ESPLOG_OFF;
    }
  }
  template <typename... T> void printf(T... arg)
  {
    if (log_level >= ESPLOG_ALL)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[debug]");
      Serial.printf(arg...);
    }
  }
  template <typename T> void println(T arg)
  {
    if (log_level >= ESPLOG_ALL)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[debug]");
      Serial.println(arg);
    }
  }
  template <typename T> void print(T arg)
  {
    if (log_level >= ESPLOG_ALL)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[debug]");
      Serial.print(arg);
    }
  }
  template <typename... T> void error_printf(T... arg)
  {
    if (log_level >= ESPLOG_ERROR)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[error]");
      Serial.printf(arg...);
    }
  }
  template <typename... T> void task_printf(T... arg)
  {
    if (log_level >= ESPLOG_TASK)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[task]");
      Serial.printf(arg...);
    }
  }
  template <typename... T> void info_printf(T... arg)
  {
    if (log_level >= ESPLOG_INFO)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[info]");
      Serial.printf(arg...);
    }
  }
  template <typename... T> void warning_printf(T... arg)
  {
    if (log_level >= ESPLOG_WARN)
    {
      Serial.printf("[%ld]", millis());
      Serial.printf("[warning]");
      Serial.printf(arg...);
    }
  }
};
extern ESPLog   esp_log;

#endif //!__UTILS_H__
