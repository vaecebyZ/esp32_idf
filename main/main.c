#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "at_uart.h"
#include "esp_log.h"
#include "at_check.h"
#include "at_sms.h"
#include "at_http.h"

static const char *TAG = "MAIN";

void get_http()
{
  at_http_get();
}

void app_main()
{
  // Initialize UART
  at_uart_init();
  while (!is_uart_inited())
  {
    ESP_LOGI(TAG, "Waiting for UART!");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // Perform AT check
  if (at_check_ping())
  {
    if (at_http_get())
    {
      at_http_get();
    }
  }
  while (1)
  {
    ESP_LOGI("STACK", "Remaining stack size: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
