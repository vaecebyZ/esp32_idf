
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>

static const char *TAG = "AT_CHECK";

bool at_check_ping()
{
  // 每个2秒发送一个AT直到返回OK (最多发送5次)
  for (int i = 0; i < 5; i++)
  {
    if (at_send_command("AT", "OK", 2000, NULL))
    {
      ESP_LOGI(TAG, "AT check passed. Module is functioning correctly.");
      return true;
    }
  }
  return false;
}

// AT Check Component
bool at_check_base()
{
  ESP_LOGI(TAG, "Performing AT check...");

  // Check AT command communication
  if (!at_send_command("AT", "OK", 5000, NULL))
  {
    ESP_LOGE(TAG, "Failed to communicate with module using AT command");
    return false;
  }

  // Retrieve ICCID
  if (!at_send_command("AT+ICCID", "+ICCID", 5000, NULL))
  {
    ESP_LOGE(TAG, "Failed to retrieve ICCID");
    return false;
  }

  // Disable echo
  if (!at_send_command("ATE0", "OK", 5000, NULL))
  {
    ESP_LOGE(TAG, "Failed to disable echo");
    return false;
  }

  // Check signal quality
  if (!at_send_command("AT+CSQ", "OK", 5000, NULL))
  {
    ESP_LOGE(TAG, "Failed to retrieve signal quality");
    return false;
  }

  // Check network attachment status
  if (!at_send_command("AT+CGATT?", "+CGATT: 1", 5000, NULL))
  {
    ESP_LOGE(TAG, "Module is not attached to the network");
    return false;
  }

  ESP_LOGI(TAG, "AT check passed. Module is functioning correctly.");
  return true;
}
