#include "at_check.h"
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#define UART_BUF_SIZE 256
static bool isPDPActive = false;
bool close()
{
  // 终止 HTTP 会话
  if (!at_send_command("AT+HTTPTERM", "OK", 10000, NULL))
  {
    ESP_LOGW("HTTP", "Failed to terminate HTTP session");
    // 终止任务或返回错误
    return false;
  }

  vTaskDelay(pdMS_TO_TICKS(3000));

  return true;
}

bool at_http_get()
{
  char response[UART_BUF_SIZE];
  int method, status_code, data_len;

  // 基本检查
  if (!at_check_base())
    return false;
  // PDP 检查
  if (!at_check_pdp())
    return false;
  // 初始化 HTTP
  if (!at_send_command("AT+HTTPINIT", "OK", 1000, NULL))
  {
    ESP_LOGE("HTTP", "Failed to initialize HTTP");
    return false;
  }
  // 设置 PDP 上下文为 CID 1
  if (!at_send_command("AT+HTTPPARA=\"CID\",1", "OK", 1000, NULL))
  {
    ESP_LOGE("HTTP", "Failed to set HTTP CID");
    close();
    return false;
  }
  // 设置目标URL
  char url[256];
  snprintf(url, sizeof(url), "AT+HTTPPARA=\"URL\",\"https://dev.usemock.com/6782c14e1f946a67671573e2/ping\"");
  if (!at_send_command(url, "OK", 15000, NULL))
  {
    ESP_LOGE("HTTP", "Failed to set HTTP URL");
    close();
    return false;
  }
  // 执行 HTTP GET
  if (!at_send_command("AT+HTTPACTION=0", "+HTTPACTION:", 3000, response))
  {
    ESP_LOGE("HTTP", "HTTP GET action failed");
    close();
    return false;
  }
  if (strlen(response) == 0)
  {
    ESP_LOGE("HTTP", "Empty response received for HTTPACTION");
    close();
    return false;
  }
  if (strstr(response, "+HTTPACTION:") != NULL)
  {
    ESP_LOGI("HTTP", "HTTPACTION response: %s", response);
    char *action_start = strstr(response, "+HTTPACTION:");
    if (action_start)
    {
      if (sscanf(action_start, "+HTTPACTION: %d,%d,%d", &method, &status_code, &data_len) == 3)
      {
        ESP_LOGI("HTTP", "Parsed HTTPACTION: method=%d, status=%d, length=%d", method, status_code, data_len);
      }
      else
      {
        ESP_LOGE("HTTP", "Failed to parse HTTPACTION response: %s", response);
        close();
        return false;
      }
    }
  }
  else
  {
    ESP_LOGE("HTTP", "Expected +HTTPACTION not found in response: %s", response);
    close();
    return false;
  }
  if (status_code != 200)
  {
    ESP_LOGE("HTTP", "HTTP request failed with status code: %d", status_code);
    close();
    return false;
  }
  ESP_LOGI("HTTP", "HTTP request succeeded, data length: %d", data_len);
  // 读取 HTTP 响应
  if (data_len > 0)
  {
    char read_command[64];
    snprintf(read_command, sizeof(read_command), "AT+HTTPREAD=0,%d", data_len);
    if (!at_send_command(read_command, "OK", 13000, response))
    {
      ESP_LOGE("HTTP", "Failed to read HTTP response");
      close();
      return false;
    }
    ESP_LOGI("HTTP", "HTTP response: %s", response);
    // 处理http get 返回
  }
  else
  {
    ESP_LOGE("HTTP", "No data available to read");
    close();
    return false;
  }
  close();
  return true;
}
