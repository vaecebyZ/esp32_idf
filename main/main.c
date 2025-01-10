#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main()
{
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "hello world");
  }
}
