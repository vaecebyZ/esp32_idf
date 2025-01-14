#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "at_uart.h"
#include "esp_log.h"
#include "at_check.h"
#include "at_sms.h"
#include "at_http.h"
#include "at_mq.h"

static const char *TAG = "MAIN";

void getMQ()
{
  at_mq_connect("h");
  at_mq_subscribe("h");
  at_mq_publish("h", "h");
  at_mq_free();
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(100)); // 避免任务占用过多 CPU
  }
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
    // char *iccid = at_get_iccid();
    // ESP_LOGI(TAG, "ICCID: %s", iccid);
    // at_http_get("https://dev.usemock.com/6782c14e1f946a67671573e2/ping");
    // at_http_post("https://dev.usemock.com/6782c14e1f946a67671573e2/pong");
    xTaskCreatePinnedToCore(getMQ, "getMQ", 8192, NULL, 5, NULL, 1);
  }
  while (1)
  {
    ESP_LOGI("STACK", "Remaining stack size: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
