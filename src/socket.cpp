#include "main.h"
#include "utils.h"
#include "oled.h"
#include "data.h"
#include "led.h"
#include "socket.h"
#include "modules/ArduinoJson/ArduinoJson.h"
#include "blinker_esp32.h"
TaskHandle_t udp_run;
TaskHandle_t tcp_run;
TaskHandle_t udp_config_run;
char         str1[60];
char         str2[60];
char         str3[60];
char         str_timeinfo[60];
char         str_clockinfo[60];

void getStatePack(String &pack)
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }

  strftime(str_timeinfo, 100, "%a", &timeinfo);
  sprintf(str1,
          "%4d-%02d-%02d %s",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          str_timeinfo); // 整合字符串
  strftime(str_clockinfo, 100, "%H:%M:%S", &timeinfo);
  if (context.rgb_running == 0)
  {
    bool online_state = false;
#ifdef USE_BLINKER
    online_state = WiFi.status() == WL_CONNECTED && context.is_mqtt_connect;
#else
    online_state = WiFi.status() == WL_CONNECTED;
#endif
    if (online_state)
    {
      sprintf(str2, "%s 在线", str_clockinfo);
    }
    else
    {
      sprintf(str2, "%s 离线", str_clockinfo);
    }
  }
  else
  {
    sprintf(str2, "%s USB", str_clockinfo);
  }
  if (timeinfo.tm_sec % 10 >= 5)
  {
    sprintf(str3, "%s|%s℃ %s", text_final, temp_final, aqi_final);
  }
  else
  {
    sprintf(str3, "%s|%s%% %s", text_final, humidity_final, category_final);
  }
  LedState leds_temp_state;
  if (!xQueuePeek(leds_queue, &leds_temp_state, 100) == pdTRUE)
  {
    memset(&leds_temp_state, 0, sizeof(LedState));
  }
  CRGB *leds_temp = leds_temp_state.leds;

  DynamicJsonDocument response_dto(10240);
  response_dto["str1"]             = str1;
  response_dto["str2"]             = str2;
  response_dto["str3"]             = str3;
  response_dto["str4"]             = hitokoto_final;
  response_dto["state"]            = context.light_on;
  response_dto["rgb"]              = context.rgb_running;
  response_dto["light_mode"]       = context.mode;
  response_dto["light_brightness"] = context.brightness;
  JsonArray leds                   = response_dto.createNestedArray("leds");
  for (int i = 0; i < 120; i++)
  {
    uint32_t color = 0;
    color          = leds_temp[i].b | leds_temp[i].g << 8 | leds_temp[i].r << 16;
    leds.add(color);
  }
  serializeJson(response_dto, pack);
}
void sendStateToAndroid(const char *host)
{
  Udp.beginPacket(host, ANDROID_PORT); // 配置远端ip地址和端口
  String response_dto = "";
  getStatePack(response_dto);
  Udp.print(response_dto);
  Udp.endPacket();
  esp_log.task_printf("state -> UDP\n");
}
void sendStateToPC(const char *host)
{
  // Udp.beginPacket(host, COMPUTER_PORT); // 配置远端ip地址和端口
  // String response_dto = "";
  // getStatePack(response_dto);
  // Udp.print(response_dto);
  // Udp.endPacket();
  // esp_log.task_printf("statePC -> UDP\n");
}
void udpTask(void *xTaskUdp)
{
  SysContext context;
  if (Udp.begin(ESP32_OLED_PORT))
  { // 启动Udp监听服务
    esp_log.println("监听成功");
    // 打印本地的ip地址，在UDP工具中会使用到
    //  WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
    esp_log.printf("现在收听IP：%s, UDP端口：%d\n",
                   WiFi.localIP().toString().c_str(),
                   ESP32_OLED_PORT);
  }
  else
  {
    esp_log.error_printf("监听失败\n");
  }
  while (1)
  {
    delay(10);
    try
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        int packetSize = Udp.parsePacket(); // 获得解析包
        if (packetSize)                     // 解析包不为空
        {
          esp_log.info_printf("Freeheap:%d\n", xPortGetFreeHeapSize());
          esp_log.printf("%s:%d(%d)->UDP:\n",
                         Udp.remoteIP().toString().c_str(),
                         Udp.remotePort(),
                         packetSize);
          char incomingPacket[255];
          int  len = Udp.read(incomingPacket, 255); // 返回数据包字节数
          if (len > 0)
          {
            esp_log.printf("%s\n", incomingPacket); // 向串口打印信息
            incomingPacket[len] = 0;                // 清空缓存
            if (strcmp(incomingPacket, "turn_on") == 0)
            {
              esp_log.task_printf("UDP -> miot\n");
              miotPowerState("on");
            }
            else if (strcmp(incomingPacket, "turn_off") == 0)
            {
              esp_log.task_printf("UDP -> miot\n");
              miotPowerState("off");
            }
            else if (strcmp(incomingPacket, "computer") == 0)
            {
              esp_log.task_printf("UDP -> miot\n");
              miotMode(BLINKER_CMD_MIOT_COMPUTER);
            }
            else if (strcmp(incomingPacket, "normal_light") == 0)
            {
              esp_log.task_printf("UDP -> miot\n");
              miotMode(BLINKER_CMD_MIOT_DAY);
            }
            else if (strcmp(incomingPacket, "computer?") == 0)
            {
              Udp.beginPacket(Udp.remoteIP().toString().c_str(),
                              COMPUTER_PORT); // 配置远端ip地址和端口
              if (!get_context(context))
              {
                continue;
              }
              if (context.rgb_running == 1)
              {
                Udp.print("computer->Y");
              }
              else
              {
                Udp.print("computer->N");
              }
              Udp.endPacket();
            }
            else if (strcmp(incomingPacket, "state") == 0)
            {
              esp_log.task_printf("UDP -> state\n");
            }
            sendStateToAndroid(Udp.remoteIP().toString().c_str());
            sendStateToPC(Udp.remoteIP().toString().c_str());
          }
        }
      }
    }
    catch (const std::exception &e)
    {
      esp_log.error_printf(e.what());
    }
  }
}
void tcpTask(void *xTaskTcp)
{
  SysContext context;

  WiFiServer TcpServer;
  WiFiClient TcpClients[MAX_SRV_CLIENTS];
  TcpServer.begin(ESP32_OLED_PORT);
  TcpServer.setNoDelay(true);
  esp_log.print(WiFi.localIP().toString());
  while (1)
  {

    int i = 0;
    // 有新连接
    if (TcpServer.hasClient())
    { // 判断是否有新的client请求进来
      for (i = 0; i < MAX_SRV_CLIENTS; i++)
      {

        // 释放旧无效或者断开的client
        if (!TcpClients[i] || !TcpClients[i].connected())
        {
          if (!TcpClients[i])
          {
            // TcpClients[i]    判断指定序号的客户端是否有效
            TcpClients[i].stop(); // 停止指定客户端的连接
          }

          TcpClients[i] = TcpServer.available(); // 分配最新的client
          esp_log.print("1个新的客户端: ");
          esp_log.println(i);
          break; // 跳出一层for循环
        }
      }

      // 当达到最大连接数 无法释放无效的client，需要拒绝连接
      if (i == MAX_SRV_CLIENTS)
      {
        WiFiClient client = TcpServer.available();
        client.stop();
        Serial.println("连接被拒绝 ");
      }
    }

    // 检测client发过来的数据
    for (i = 0; i < MAX_SRV_CLIENTS; i++)
    {
      if (TcpClients[i] && TcpClients[i].connected())
      {
        if (TcpClients[i].available())
        {
          String msg = TcpClients[i].readString();
          esp_log.print(msg);
          if (msg == "turn_on")
          {
            esp_log.task_printf("TCP -> miot\n");
            miotPowerState("on");
          }
          else if (msg == "turn_off")
          {
            esp_log.task_printf("TCP -> miot\n");
            miotPowerState("off");
          }
          else if (msg == "computer")
          {
            esp_log.task_printf("TCP -> miot\n");
            miotMode(BLINKER_CMD_MIOT_COMPUTER);
          }
          else if (msg == "normal_light")
          {
            esp_log.task_printf("TCP -> miot\n");
            miotMode(BLINKER_CMD_MIOT_DAY);
          }
          else if (msg == "computer?")
          {
            if (!get_context(context))
            {
              continue;
            }
            if (context.rgb_running == 1)
            {
              TcpClients[i].print("computer->Y");
            }
            else
            {
              TcpClients[i].print("computer->N");
            }
          }
        }
      }
    }
    delay(10);
  }
}
void udpConfigTask(void *xTaskConfigUdp)
{
  if (Udp.begin(ESP32_OLED_PORT))
  { // 启动Udp监听服务
    esp_log.println("监听成功");
    // 打印本地的ip地址，在UDP工具中会使用到
    //  WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
    esp_log.printf("现在收听IP：%s, UDP端口：%d\n",
                   WiFi.localIP().toString().c_str(),
                   ESP32_OLED_PORT);
    sendStateToAndroid("255.255.255.255");
    sendStateToPC("255.255.255.255");
  }
  else
  {
    esp_log.error_printf("监听失败\n");
  }
  while (1)
  {

    delay(100);
    int packetSize = Udp.parsePacket(); // 获得解析包
    if (packetSize)                     // 解析包不为空
    {

      esp_log.printf("%s:%d(%d)->UDP:\n",
                     Udp.remoteIP().toString().c_str(),
                     Udp.remotePort(),
                     packetSize);
      char   incomingPacket[255];
      int    len    = Udp.read(incomingPacket, 255); // 返回数据包字节数
      String packet = incomingPacket;
      if (len > 0)
      {
        esp_log.printf("%s\n", incomingPacket); // 向串口打印信息
        incomingPacket[len] = 0;                // 清空缓存
        Udp.flush();
        if (strcmp(incomingPacket, "state") == 0)
        {
          esp_log.task_printf("UDP -> state\n");
          Udp.beginPacket(Udp.remoteIP().toString().c_str(),
                          ANDROID_PORT); // 配置远端ip地址和端口
          DynamicJsonDocument response_dto(10240);
          char                str_connect[100];
          sprintf(str_connect, "%d台设备已连接", WiFi.softAPgetStationNum());
          response_dto["str1"]             = DEVICE_NAME;
          response_dto["str2"]             = "配网模式beta";
          response_dto["str3"]             = "启动app完成操作";
          response_dto["str4"]             = str_connect;
          response_dto["state"]            = -1;
          response_dto["rgb"]              = -1;
          response_dto["light_mode"]       = -1;
          response_dto["light_brightness"] = -1;
          JsonArray leds                   = response_dto.createNestedArray("leds");
          serializeJson(response_dto, Udp);
          Udp.endPacket();
          esp_log.task_printf("state -> UDP\n");
        }
        else if (packet.indexOf("{") != -1)
        {
          DynamicJsonDocument doc(1024);
          deserializeJson(doc, packet);
          String ssid = doc["ssid"];
          String pwd  = doc["pwd"];
          if (!esp_EEPROM.begin(1024))
          {
            esp_log.println("eeprom fail!");
            oled_show(DEVICE_NAME, "eeprom错误", "请等待", "正在重启...");
            delay(100);
            esp_restart();
          } // 自定义从3000开始
          oled_show(DEVICE_NAME, "已收到配网信息", "请等待", "正在重启...");
          esp_EEPROM.writeString(100, ssid);
          esp_EEPROM.writeString(200, pwd);
          esp_EEPROM.commit();
          esp_EEPROM.end();
          delay(100);
          esp_restart();
        }
      }
    }
  }
}