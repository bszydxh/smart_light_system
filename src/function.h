#ifndef FUNCTION_H
#define FUNCTION_H
#include "arduino.h"
#define DEST_FS_USES_LITTLEFS
#include <ESP32-targz.h> //解析gzip
#define ESPLOG_ALL 6
#define ESPLOG_DEBUG 5
#define ESPLOG_INFO 4
#define ESPLOG_TASK 3
#define ESPLOG_WARN 2
#define ESPLOG_ERROR 1
#define ESPLOG_OFF 0
class LightSet
{
    int light_set_num = 0; // 通信量
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
    template <typename... T>
    void printf(T... arg)
    {
        if (log_level >= ESPLOG_ALL)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[debug]");
            Serial.printf(arg...);
        }
    }
    template <typename T>
    void println(T arg)
    {
        if (log_level >= ESPLOG_ALL)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[debug]");
            Serial.println(arg);
        }
    }
    template <typename T>
    void print(T arg)
    {
        if (log_level >= ESPLOG_ALL)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[debug]");
            Serial.print(arg);
        }
    }
    template <typename... T>
    void error_printf(T... arg)
    {
        if (log_level >= ESPLOG_ERROR)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[error]");
            Serial.printf(arg...);
        }
    }
    template <typename... T>
    void task_printf(T... arg)
    {
        if (log_level >= ESPLOG_TASK)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[task]");
            Serial.printf(arg...);
        }
    }
    template <typename... T>
    void info_printf(T... arg)
    {
        if (log_level >= ESPLOG_INFO)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[info]");
            Serial.printf(arg...);
        }
    }
    template <typename... T>
    void warning_printf(T... arg)
    {
        if (log_level >= ESPLOG_WARN)
        {
            Serial.printf("[%ld]", millis());
            Serial.printf("[warning]");
            Serial.printf(arg...);
        }
    }
};

class ESPGZIP
{
    GzUnpacker *GZUnpacker;

public:
    void setup()
    {
        GZUnpacker = new GzUnpacker();
        GZUnpacker->haltOnError(true);                                            // stop on fail (manual restart/reset required)
        GZUnpacker->setupFSCallbacks(targzTotalBytesFn, targzFreeBytesFn);        // prevent the partition from exploding, recommended
        GZUnpacker->setGzProgressCallback(BaseUnpacker::defaultProgressCallback); // targzNullProgressCallback or defaultProgressCallback
        GZUnpacker->setLoggerCallback(BaseUnpacker::targzPrintLoggerCallback);    // gz log verbosity
    }
    void ungzip(Stream *stream)
    {
        if (!GZUnpacker->gzStreamUpdater(stream, UPDATE_SIZE_UNKNOWN))
        {
            Serial.printf("gzStreamUpdater failed with return code #%d\n", GZUnpacker->tarGzGetError());
        } // tar log verbosity
    }
};
#endif