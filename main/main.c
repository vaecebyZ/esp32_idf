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
#include "at_config.h"
#include "cJSON.h"
#include "at_utils.h"

static const char *TAG = "MAIN";

void getMQ()
{
  char *iccid = at_get_iccid();
  if (iccid == NULL)
  {
    ESP_LOGI(TAG, "ICCID is NULL");
    return;
  }
  mqConfig_t mqconfig = {
      .server = "",
      .port = "",
      .clientId = iccid,
      .username = "",
      .password = "",
  };
  at_mq_connect(mqconfig);
  char topic[UART_BUF_SIZE];
  sprintf(topic, "/platform/%s/regist/#", mqconfig.username);
  at_mq_subscribe(topic);
  cJSON *payload = cJSON_CreateObject();
  cJSON_AddStringToObject(payload, "deviceId", iccid);
  cJSON_AddStringToObject(payload, "deviceName", "ESP32_AT");
  cJSON_AddStringToObject(payload, "deviceCate", getDeviceCateString(Elevator));
  cJSON_AddStringToObject(payload, "mqttUserName", mqconfig.username);
  cJSON_AddStringToObject(payload, "projectInfoCode", "PJ202406050002");
  char uuid[37];
  generate_random_uuid(uuid, sizeof(uuid));
  sprintf(topic, "/platform/%s/regist", mqconfig.username);
  mqMessage_t message = {
      .topic = topic,
      .event = RegistDevice,
      .data = payload,
      .time = get_current_timestamp_ms(),
      .ttl = 5000,
      .id = uuid,
  };
  at_mq_publish(message, NULL, NULL);
  at_mq_listening();
  while (1)
  {
    ESP_LOGI("STACK", "Remaining stack size: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(10000));
  }

  // at_mq_free();
  // vTaskDelete(NULL);
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

    initSysTimeByAT();
    // Print current time
    // char *iccid = at_get_iccid();
    // ESP_LOGI(TAG, "ICCID: %s", iccid);
    // at_http_get("https://dev.usemock.com/6782c14e1f946a67671573e2/ping");
    // at_http_post("https://dev.usemock.com/6782c14e1f946a67671573e2/pong");
    xTaskCreatePinnedToCore(getMQ, "getMQ", 8192, NULL, 5, NULL, 1);
  }
}
