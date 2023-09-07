#include "main.h"
#include "utils.h"
#include "button.h"
#include "blinker_esp32.h"

TaskHandle_t button_run;
TaskHandle_t button_config_run;

void buttonTask(void *xTaskButton)
{
  SysContext context;
  uint8_t    button2_pin    = 34;
  uint8_t    button1_pin    = 35;
  int        button1_now    = 0;
  int        button2_now    = 0;
  int        button1_before = 0;
  int        button2_before = 0;
  pinMode(button2_pin, INPUT);
  pinMode(button1_pin, INPUT);
  while (1)
  {
    button1_now = digitalRead(button1_pin);
    button2_now = digitalRead(button2_pin);
    if (button1_now == 0 && button1_before == 1)
    {
      esp_log.printf("\n1up\n");
    }
    else if (button1_now == 1 && button1_before == 0)
    {
      esp_log.printf("\n1down\n");
      if (!get_context(context))
      {
        continue;
      }
      if (context.light_on == 1)
      {
        esp_log.task_printf("button1 -> miot\n");
        miotPowerState("off");
      }
      else if (context.light_on == 0)
      {
        esp_log.task_printf("button1 -> miot\n");
        miotPowerState("on");
      }
    }
    if (button2_now == 0 && button2_before == 1)
    {
      esp_log.printf("\n2up\n");
    }
    else if (button2_now == 1 && button2_before == 0)
    {
      if (!get_context(context))
      {
        continue;
      }
      if (context.mi_mode == BLINKER_CMD_MIOT_DAY)
      {
        esp_log.task_printf("button2 -> miot\n");
        miotMode(BLINKER_CMD_MIOT_COMPUTER);
      }
      else if (context.mi_mode == BLINKER_CMD_MIOT_COMPUTER)
      {
        esp_log.task_printf("button2 -> miot\n");
        miotMode(BLINKER_CMD_MIOT_DAY);
      }
      else
      {
        esp_log.task_printf("button2 -> miot\n");
        miotMode(BLINKER_CMD_MIOT_DAY);
      }
      esp_log.printf("\n2down\n");
    }
    button1_before = button1_now;
    button2_before = button2_now;
    delay(10);
  }
}
void buttonConfigTask(void *xTaskButtonConfig)
{
  uint8_t button2_pin    = 34;
  uint8_t button1_pin    = 35;
  int     button1_now    = 0;
  int     button2_now    = 0;
  int     button1_before = 0;
  int     button2_before = 0;
  pinMode(button2_pin, INPUT);
  pinMode(button1_pin, INPUT);
  while (1)
  {
    button1_now = digitalRead(button1_pin);
    button2_now = digitalRead(button2_pin);
    if (button1_now == 0 && button1_before == 1)
    {
      esp_log.printf("\n1up\n");
    }
    else if (button1_now == 1 && button1_before == 0)
    {
      esp_log.printf("\n1down\n");
      int system_state = CONFIG_SETUP;
      xQueueOverwrite(system_state_queue, &system_state);
    }
    if (button2_now == 0 && button2_before == 1)
    {
      esp_log.printf("\n2up\n");
    }
    else if (button2_now == 1 && button2_before == 0)
    {
      esp_restart();
      esp_log.printf("\n2down\n");
    }
    button1_before = button1_now;
    button2_before = button2_now;
    delay(10);
  }
}