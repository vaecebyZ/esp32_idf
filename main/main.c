#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "at_uart.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main()
{

  ESP_LOGI(TAG, "hello world");
  vTaskDelay(pdMS_TO_TICKS(1000));
  at_uart_init();
  while (!is_uart_inited())
  {
    ESP_LOGI(TAG, "Waiting for UART!");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
