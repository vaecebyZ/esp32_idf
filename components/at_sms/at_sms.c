#include "at_check.h"
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
static const char *TAG = "AT_SMS";

// Helper function to convert string to UCS2 hex encoding (binary format)
void string_to_ucs2_binary(const char *input, uint8_t *output, size_t *output_len)
{
  *output_len = 0; // 初始化输出长度

  while (*input)
  {
    // 将每个字符转换为 UCS2 (16-bit) 的高字节和低字节
    uint16_t ucs2_char = (uint16_t)(unsigned char)*input; // 单字节转 16 位宽字符
    output[(*output_len)++] = (ucs2_char >> 8) & 0xFF;    // 高字节
    output[(*output_len)++] = ucs2_char & 0xFF;           // 低字节
    input++;
  }
}
// Send an SMS message to a phone number
bool at_sms_send()
{
  // Base check
  if (!at_check_base())
    return false;
  // Execute AT commands step by step
  if (!at_send_command("AT+CSMS=1", "OK", 1000, NULL))
    return false;
  if (!at_send_command("AT+CMGF=1", "OK", 1000, NULL))
    return false;
  if (!at_send_command("AT+CMGS=\"1390000000\"", ">", 3000, NULL))
    return false;
  if (!at_send_command("HELLOW", "OK", 15000, NULL))
  {
    ESP_LOGE(TAG, "Failed to send SMS content");
    return false;
  }

  ESP_LOGI(TAG, "SMS sent!");
  return true;
}
