#ifndef __DATA_H__
#define __DATA_H__

extern String eeprom_ssid;
extern String eeprom_pwd;

extern char text_final[30];
extern char covid_final[30];
extern char temp_final[10];
extern char humidity_final[10];
extern char aqi_final[10];
extern char category_final[30];
extern char hitokoto_final[100];

#if __has_include("xd.h") // 非c++官方用法，xd.h是项目作者自己的魔改部分，看情况删
extern char xd_final[30];
#endif

extern int         time_hour;
extern int         time_min;
extern int         time_all;
extern EEPROMClass esp_EEPROM;
void               EEPROM_rgb_memory_commit();
void               EEPROM_setup();
#endif //!__DATA_H__