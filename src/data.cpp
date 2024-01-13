#include "main.h"
#include "utils.h"
#include "data.h"
String eeprom_ssid;
String eeprom_pwd;

char text_final[30]      = "";
char covid_final[30]     = "";
char temp_final[10]      = "";
char humidity_final[10]  = "";
char aqi_final[10]       = "";
char category_final[30]  = "";
char hitokoto_final[100] = "松花酿酒,春水煎茶。";

int time_hour = 0; // 通信量
int time_min  = 0; // 通信量
int time_all  = 0; // 通信量

// eeprom掉电保护部分
EEPROMClass esp_EEPROM("esp_EEPROM");

void EEPROM_rgb_memory_commit()
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  int light_color_r_index[3];
  int light_color_g_index[3];
  int light_color_b_index[3];
  int light_brightness_index = context.brightness;
  for (int i = 0; i < 3; i++)
  {
    light_color_r_index[i] = context.color_r[i];
    light_color_g_index[i] = context.color_g[i];
    light_color_b_index[i] = context.color_b[i];
  }
  if (!esp_EEPROM.begin(1024))
  {
    esp_log.println("eeprom fail!");
    delay(100);
    esp_restart();
  }
  esp_EEPROM.put(0, light_color_r_index[0]);
  esp_EEPROM.put(4, light_color_g_index[0]);
  esp_EEPROM.put(8, light_color_b_index[0]);
  esp_EEPROM.put(12, light_color_r_index[1]);
  esp_EEPROM.put(16, light_color_g_index[1]);
  esp_EEPROM.put(20, light_color_b_index[1]);
  esp_EEPROM.put(24, light_color_r_index[2]);
  esp_EEPROM.put(28, light_color_g_index[2]);
  esp_EEPROM.put(32, light_color_b_index[2]);
  esp_EEPROM.put(36, light_brightness_index);
  esp_EEPROM.commit();
  esp_log.printf("eeprom rgb commit success!\n");
  esp_log.printf("rgb1:%d:%d:%d\n",
                 light_color_r_index[0],
                 light_color_g_index[0],
                 light_color_b_index[0]);
  esp_log.printf("rgb2:%d:%d:%d\n",
                 light_color_r_index[1],
                 light_color_g_index[1],
                 light_color_b_index[1]);
  esp_log.printf("rgb3:%d:%d:%d\n",
                 light_color_r_index[2],
                 light_color_g_index[2],
                 light_color_b_index[2]);
  esp_log.printf("bright:%d\n", light_brightness_index);
  esp_EEPROM.end();
}
void EEPROM_setup()
{
  SysContext context;
  if (!get_context(context))
  {
    return;
  }
  if (!esp_EEPROM.begin(1024))
  {
    esp_log.error_printf("eeprom fail!\n");
    delay(100);
    esp_restart();
  } // 自定义从3000开始
  for (int i = 0; i <= 36; i = i + 4)
  {
    if (esp_EEPROM.readInt(i) == -1)
    {
      esp_EEPROM.put(i, 255);
      break;
    }
  }
  esp_log.printf("?ssid: %s, pwd: %s\n",
                 esp_EEPROM.readString(100),
                 esp_EEPROM.readString(200));
  if (esp_EEPROM.readString(100).isEmpty())
  {
    esp_EEPROM.writeString(100, "unknown_ssid");
  }
  if (esp_EEPROM.readString(200).isEmpty())
  {
    esp_EEPROM.writeString(200, "unknown_pwd");
  }
  esp_EEPROM.commit();
  context.color_r[0] = esp_EEPROM.readInt(0);
  context.color_g[0] = esp_EEPROM.readInt(4);
  context.color_b[0] = esp_EEPROM.readInt(8);
  context.color_r[1] = esp_EEPROM.readInt(12);
  context.color_g[1] = esp_EEPROM.readInt(16);
  context.color_b[1] = esp_EEPROM.readInt(20);
  context.color_r[2] = esp_EEPROM.readInt(24);
  context.color_g[2] = esp_EEPROM.readInt(28);
  context.color_b[2] = esp_EEPROM.readInt(32);
  context.brightness = esp_EEPROM.readInt(36);
  eeprom_ssid        = esp_EEPROM.readString(100);
  eeprom_pwd         = esp_EEPROM.readString(200);
  esp_log.printf("ssid: %s, pwd: %s\n", eeprom_ssid, eeprom_pwd);
  esp_log.printf("rgb1:%d:%d:%d\n",
                 context.color_r[0],
                 context.color_g[0],
                 context.color_b[0]);
  esp_log.printf("rgb2:%d:%d:%d\n",
                 context.color_r[1],
                 context.color_g[1],
                 context.color_b[1]);
  esp_log.printf("rgb3:%d:%d:%d\n",
                 context.color_r[2],
                 context.color_g[2],
                 context.color_b[2]);
  esp_log.printf("bright:%d\n", context.brightness);
  set_context(context);
}