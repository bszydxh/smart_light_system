#include "main.h"
#include "utils.h"
#include "data.h"
#include "oled.h"
#include "http.h"
#include "ArduinoJson.h"

#if __has_include("xd.h") // 非c++官方用法，xd.h是项目作者自己的魔改部分，看情况删
#include "xd.h"
#endif

extern TaskHandle_t http_run;
TaskHandle_t http_run;
HTTPClient httpClient;
int http_get(HTTPClient &http)
{
  // vTaskSuspend(blinker_run);
  int httpCode = http.GET();
  // vTaskResume(blinker_run);
  return httpCode;
}
void esp32_Http_aqi()
{
  // HTTPClient httpClient;
  // httpClient.begin(AIR_URL);
  // httpClient.setUserAgent(HTTP_USERAGENT);
  // httpClient.addHeader("charset", "utf-8");
  // int httpCode = http_get(httpClient);
  // esp_log.task_printf("esp32 -> aqi server\n");
  // if (httpCode == HTTP_CODE_OK)
  // {
  //     esp_log.task_printf("aqi server -> esp32:\n");
  //     DynamicJsonDocument jsonBuffer(2048);
  //     String str = Zlib::ungzipToString(httpClient.getStream());
  //     esp_log.println(str);
  //     deserializeJson(jsonBuffer, str);
  //     JsonObject root = jsonBuffer.as<JsonObject>();
  //     const char *category = root["now"]["category"];
  //     const char *aqi = root["now"]["aqi"];
  //     if (category != NULL && aqi != NULL)
  //     {
  //         sprintf(aqi_final, "%s", aqi);
  //         sprintf(category_final, "%s", category);
  //     }
  //     esp_log.println("url2 get");
  // }
  // else
  // {
  //     esp_log.task_printf("aqi server -/> esp32:\n");
  //     esp_log.println(httpCode);
  // }
  // httpClient.end();
}
void esp32_Http_weather(HTTPClient& httpClient,DynamicJsonDocument& jsonBuffer)
{
  httpClient.begin(WEATHER_URL);
  httpClient.setUserAgent(HTTP_USERAGENT);
  httpClient.addHeader("charset", "utf-8");
  int httpCode = http_get(httpClient);
  esp_log.task_printf("esp32 -> weather server:\n");
  if (httpCode == HTTP_CODE_OK)
  {
    esp_log.task_printf("weather server -> esp32:\n");
    
    deserializeJson(jsonBuffer, httpClient.getStream());
    JsonObject root = jsonBuffer.as<JsonObject>();
    const char *text = root["results"][0]["now"]["text"];
    const char *temp = root["results"][0]["now"]["temperature"];
    if (text != NULL)
    {
      sprintf(text_final, "%s", text);
    }
    if (temp != NULL)
    {
      sprintf(temp_final, "%s", temp);
    }
    // if (humidity != NULL)
    // {
    //   sprintf(humidity_final, "%s", humidity);
    // }
    esp_log.println(httpClient.getString());
  }
  else
  {
    esp_log.task_printf("weather server -/> esp32:\n");
    esp_log.println(httpCode);
  }
  httpClient.end();
}
void esp32_Http_hitokoto(HTTPClient& httpClient2)
{
  httpClient2.begin("https://v1.hitokoto.cn/?encode=text&max_length=10");
  httpClient2.setUserAgent(HTTP_USERAGENT);
  httpClient2.addHeader("charset", "utf-8");
  int httpCode2 = http_get(httpClient2);
  esp_log.task_printf("esp32 -> hitokoto server\n");
  if (httpCode2 == HTTP_CODE_OK)
  {
    const String &payload2 = httpClient2.getString();
    esp_log.task_printf("hitokoto server -> esp32:\n");
    esp_log.println(payload2);
    if (payload2 != NULL)
    {
      sprintf(hitokoto_final, "%s", payload2.c_str());
    }
  }
  else
  {
    esp_log.task_printf("hitokoto server /> esp32:\n");
    esp_log.println(httpCode2);
  }
  httpClient2.end();
}

void httpTask(void *xTaskHttp) // 巨型http请求模块任务,掌管http模块监控服务
{
  int system_state = NOT_SETUP;
  int system_state_last = NOT_SETUP;
  DynamicJsonDocument jsonBuffer(2048);
  while (1)
  {
    delay(400);
    system_state_last = system_state;
    xQueuePeek(system_state_queue, &system_state, 100);
    if (system_state == NOT_SETUP)
    // http是服务端主体业务，由于blinker就一个黑盒，放在自己写的http服务下面
    {
      if (timeinfo.tm_year <= 100 || !strcmp(hitokoto_final, "松花酿酒,春水煎茶。"))
      {
        esp_log.println("configDNS&Time!");
        if (millis() > 100000)
        {
          oled_show(DEVICE_NAME, "系统错误", "dns配置失败", "正在重启...");
          WiFi.disconnect();
          delay(300);
          timeinfo.tm_year = 0;
          esp_restart();
        }
        configTime(gmtOffset_sec,
                   daylightOffset_sec,
                   ntpServer1,
                   ntpServer2,
                   ntpServer3);
        if (!getLocalTime(&timeinfo))
        {
          continue;
        }
      }
      // esp32_Http_aqi();
      esp32_Http_hitokoto(httpClient); // 获取一言
      esp32_Http_weather(httpClient,jsonBuffer);
#if __has_include("xd.h")
      // esp32_Http_XD();
#endif
      system_state = FINISH_SETUP;
      xQueueOverwrite(system_state_queue, &system_state);
    }
    if (system_state == FINISH_SETUP)
    {
      if (timeinfo.tm_sec == 10)
      {
        if (timeinfo.tm_min % 4 == 3)
        {
          
        }
        if (timeinfo.tm_min % 4 == 1)
        {
          // esp32_Http_aqi();
          esp32_Http_weather(httpClient,jsonBuffer);
        }
        if (timeinfo.tm_min % 4 == 2)
        {
          esp32_Http_hitokoto(httpClient);
        }
        if (timeinfo.tm_min % 4 == 3)
        {
#if __has_include("xd.h")
          // esp32_Http_XD();
#endif
        }
      }
    }
  }
}