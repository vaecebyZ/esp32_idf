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
    // at_http_get("https://dev.usemock.com/6782c14e1f946a67671573e2/ping");

    at_http_post("https://dev.usemock.com/6782c14e1f946a67671573e2/pong");
  }
  while (1)
  {
    ESP_LOGI("STACK", "Remaining stack size: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
