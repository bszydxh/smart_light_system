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
    void setup()
    {
        Serial.begin(460800);
        delay(500);
        //udp.begin(7777);
    }
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
    int printf(const char *format, ...)
    {
        if (log_out != 0)
        {
            char loc_buf[64];
            char *temp = loc_buf;
            va_list arg;
            va_list copy;
            va_start(arg, format);
            va_copy(copy, arg);
            int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
            va_end(copy);
            if (len < 0)
            {
                va_end(arg);
                return 0;
            };
            if (len >= sizeof(loc_buf))
            {
                temp = (char *)malloc(len + 1);
                if (temp == NULL)
                {
                    va_end(arg);
                    return 0;
                }
                len = vsnprintf(temp, len + 1, format, arg);
            }
            va_end(arg);
            len = Serial.write((uint8_t *)temp, len);
            if (temp != loc_buf)
            {
                free(temp);
            }
            return len;
        }
        return 0;
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
