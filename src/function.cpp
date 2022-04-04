#include "arduino.h"
class LightSet
{
    int light_set_num = 0; //通信量
public:
    void reset()
    {
        light_set_num = 0;
    }
    void next()
    {
        light_set_num++;
        if (light_set_num == 3)
        {
            light_set_num = 0;
        }
    }
    int get_num()
    {
        return light_set_num;
    }
};
class ESPLog
{
    int log_out = 1;

public:
    int get_log_out_state()
    {
        return log_out;
    }
    void set_log_out_state(int n)
    {
        if (n >= 0 && n <= 1)
        {
            log_out = n;
        }
        else
        {
            log_out = 1;
        }
    }
    void printf(const char *format, ...)
    {
        if (log_out != 0)
            Serial.print(format);
    }
    void println(const char *format)
    {
        if (log_out != 0)
            Serial.println(format);
    }
    void println(int value)
    {
        if (log_out != 0)
            Serial.println(value);
    }
    void println(const String &s)
    {
        if (log_out != 0)
            Serial.println(s);
    }
    void println(void)
    {
        if (log_out != 0)
            Serial.println();
    }
    void print(const char *format)
    {
        if (log_out != 0)
            Serial.print(format);
    }
    void print(int value)
    {
        if (log_out != 0)
            Serial.print(value);
    }
    void error_printf(const char *format, ...)
    {
            Serial.println(format);
            Serial.printf("[%ld]", millis());
            Serial.printf("[error]");
    }
    void task_printf(const char *format, ...)
    {
            Serial.printf("[%ld]", millis());
            Serial.printf("[task]");
            Serial.println(format);
    }
};
