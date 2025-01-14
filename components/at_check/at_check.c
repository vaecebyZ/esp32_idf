
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#define RESET_PIN GPIO_NUM_1
#define UART_BUF_SIZE 256

static const char *TAG = "AT_CHECK";
// 初始化GPIO
void init_gpio()
{
  gpio_config_t reset_cfg = {
      .pin_bit_mask = (1 << RESET_PIN),
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&reset_cfg);
}
// Reset the module set by the RESET_PIN High for 1 second
bool at_check_reset()
{
  init_gpio();
  ESP_LOGI(TAG, "Resetting module...");
  gpio_set_level(RESET_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(1000));
  gpio_set_level(RESET_PIN, 0);
  ESP_LOGI(TAG, "Module reset complete..");
  vTaskDelay(pdMS_TO_TICKS(6000));
  return true;
}

bool at_check_ping()
{
  // 每个2秒发送一个AT直到返回OK (最多发送5次)
  for (int i = 0; i < 5; i++)
  {
    if (at_send_command("AT", "OK", 2000, NULL, false))
    {
      ESP_LOGI(TAG, "AT check passed. Module is functioning correctly.");
      return true;
    }
  }
  if (at_check_reset())
  {
    return at_check_ping();
  }
  return false;
}

// AT Check Component
bool at_check_base()
{
  ESP_LOGI(TAG, "Performing AT check...");

  // Check AT command communication
  if (!at_send_command("AT", "OK", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to communicate with module using AT command");
    return false;
  }

  // Retrieve ICCID
  if (!at_send_command("AT+ICCID", "+ICCID", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to retrieve ICCID");
    return false;
  }

  // Disable echo
  if (!at_send_command("ATE0", "OK", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to disable echo");
    return false;
  }

  // Check signal quality
  if (!at_send_command("AT+CSQ", "OK", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to retrieve signal quality");
    return false;
  }

  // Check network attachment status
  if (!at_send_command("AT+CGATT?", "+CGATT: 1", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Module is not attached to the network");
    return false;
  }

  ESP_LOGI(TAG, "AT check passed. Module is functioning correctly.");
  return true;
}

static bool isPDPActive = false;
bool at_check_pdp()
{
  char response[UART_BUF_SIZE];
  // 设置 GPRS PDP 上下文 确保 PDP 激活
  if (!isPDPActive)
  {
    if (!at_send_command("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 1000, NULL, false))
    {
      ESP_LOGE(TAG, "Failed to set GPRS connection type");
      return false;
    }
    if (!at_send_command("AT+SAPBR=3,1,\"APN\",\"\"", "OK", 1000, NULL, false))
    {
      ESP_LOGE(TAG, "Failed to set APN");
      return false;
    }
    // 查询 PDP 状态，确保 IP 地址有效
    if (!at_send_command("AT+SAPBR=2,1", "+SAPBR:", 3000, response, false))
    {
      ESP_LOGE(TAG, "Failed to query PDP context status");
      return false;
    }
    ESP_LOGI(TAG, "IP context status: %s", response);
    if (strstr(response, "\"0.0.0.0\"") != NULL)
    {
      ESP_LOGE(TAG, "Invalid IP address, PDP is not Active ,Activating PDP context...");
      // 激活 PDP 上下文
      if (!at_send_command("AT+SAPBR=1,1", "OK", 3000, NULL, false))
      {
        ESP_LOGE(TAG, "Failed to activate PDP context");
        return false;
      }

      // 再次查询 PDP 状态
      if (!at_send_command("AT+SAPBR=2,1", "+SAPBR:", 3000, response,false))
      {
        ESP_LOGE(TAG, "Failed to query PDP context status");
        return false;
      }
      ESP_LOGI(TAG, "IP context status: %s", response);
      if (strstr(response, "\"0.0.0.0\"") != NULL)
      {
        ESP_LOGE(TAG, "Failed to activate PDP context");
        return false;
      }
    }
    // 设置 PDP 激活标志
    isPDPActive = true;
    return true;
  }
  else
  {
    return true;
  }
}