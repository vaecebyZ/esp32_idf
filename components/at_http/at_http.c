#include "at_check.h"
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "cJSON.h"
#include "at_config.h"

static char *TAG = "HTTP";

bool close()
{
  // 终止 HTTP 会话
  if (!at_send_command("AT+HTTPTERM", "OK", 10000, NULL, false))
  {
    ESP_LOGW(TAG, "Failed to terminate HTTP session");
    // 终止任务或返回错误
    return false;
  }

  vTaskDelay(pdMS_TO_TICKS(3000));

  return true;
}

bool at_http_get(const char *path)
{
  if (path == NULL || strlen(path) == 0)
  {
    ESP_LOGE(TAG, "Invalid path");
    return false;
  }
  char response[UART_BUF_SIZE];
  int method, status_code, data_len;

  // 基本检查
  if (!at_check_base())
    return false;
  // PDP 检查
  if (!at_check_pdp())
    return false;
  // 初始化 HTTP
  if (!at_send_command("AT+HTTPINIT", "OK", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to initialize HTTP");
    return false;
  }
  // 设置 PDP 上下文为 CID 1
  if (!at_send_command("AT+HTTPPARA=\"CID\",1", "OK", 3000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to set HTTP CID");
    close();
    return false;
  }
  // 设置目标URL
  char url[256];
  snprintf(url, sizeof(url), "AT+HTTPPARA=\"URL\",\"%s\"", path);
  if (!at_send_command(url, "OK", 15000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to set HTTP URL");
    close();
    return false;
  }
  // 执行 HTTP GET
  if (!at_send_command("AT+HTTPACTION=0", "+HTTPACTION:", 3000, response, false))
  {
    ESP_LOGE(TAG, "HTTP GET action failed");
    close();
    return false;
  }
  if (strlen(response) == 0)
  {
    ESP_LOGE(TAG, "Empty response received for HTTPACTION");
    close();
    return false;
  }
  if (strstr(response, "+HTTPACTION:") != NULL)
  {
    ESP_LOGI(TAG, "HTTPACTION response: %s", response);
    char *action_start = strstr(response, "+HTTPACTION:");
    if (action_start)
    {
      if (sscanf(action_start, "+HTTPACTION: %d,%d,%d", &method, &status_code, &data_len) == 3)
      {
        ESP_LOGI(TAG, "Parsed HTTPACTION: method=%d, status=%d, length=%d", method, status_code, data_len);
      }
      else
      {
        ESP_LOGE(TAG, "Failed to parse HTTPACTION response: %s", response);
        close();
        return false;
      }
    }
  }
  else
  {
    ESP_LOGE(TAG, "Expected +HTTPACTION not found in response: %s", response);
    close();
    return false;
  }
  if (status_code != 200)
  {
    ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    close();
    return false;
  }
  ESP_LOGI(TAG, "HTTP request succeeded, data length: %d", data_len);
  // 读取 HTTP 响应
  if (data_len > 0)
  {
    char read_command[64];
    snprintf(read_command, sizeof(read_command), "AT+HTTPREAD=0,%d", data_len);
    if (!at_send_command(read_command, "OK", 13000, response, false))
    {
      ESP_LOGE(TAG, "Failed to read HTTP response");
      close();
      return false;
    }
    ESP_LOGI(TAG, "HTTP response: %s", response);
    // 处理http get 返回
  }
  else
  {
    ESP_LOGE(TAG, "No data available to read");
    close();
    return false;
  }
  close();
  return true;
}

bool at_http_post(const char *path)
{
  if (path == NULL || strlen(path) == 0)
  {
    ESP_LOGE(TAG, "Invalid path");
    return false;
  }
  char response[UART_BUF_SIZE];
  int method, status_code, data_len;

  // 基本检查
  if (!at_check_base())
    return false;
  // PDP 检查
  if (!at_check_pdp())
    return false;
  // 初始化 HTTP
  if (!at_send_command("AT+HTTPINIT", "OK", 5000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to initialize HTTP");
    return false;
  }
  // 设置 PDP 上下文为 CID 1
  if (!at_send_command("AT+HTTPPARA=\"CID\",1", "OK", 1000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to set HTTP CID");
    close();
    return false;
  }
  // 设置http请求头
  if (!at_send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 1000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to set HTTP CONTENT");
    close();
    return false;
  }
  // 设置目标URL
  char url[256];
  snprintf(url, sizeof(url), "AT+HTTPPARA=\"URL\",\"%s\"", path);
  if (!at_send_command(url, "OK", 15000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to set HTTP URL");
    close();
    return false;
  }

  char *json_string = NULL;
  cJSON *obj = cJSON_CreateObject();
  // 添加字段
  cJSON_AddStringToObject(obj, "test", "123");
  cJSON_AddBoolToObject(obj, "bool", true);
  json_string = cJSON_PrintUnformatted(obj);
  char post_data[512];
  snprintf(post_data, sizeof(post_data), "AT+HTTPDATA=%d,10000", strlen(json_string));
  // 设置post发送数据长度和超时时间
  if (!at_send_command(post_data, "DOWNLOAD", 1000, NULL, false))
  {
    ESP_LOGE(TAG, "Failed to set HTTPDATA");
    close();
    return false;
  }

  // 发送post数据
  if (!at_send_command(json_string, "OK", 1000, NULL, true))
  {
    ESP_LOGE(TAG, "Failed to send post data");
    close();
    return false;
  }

  // 执行 HTTP POST
  if (!at_send_command("AT+HTTPACTION=1", "+HTTPACTION:", 3000, response, false))
  {
    ESP_LOGE(TAG, "HTTP POST action failed");
    close();
    return false;
  }
  if (strlen(response) == 0)
  {
    ESP_LOGE(TAG, "Empty response received for HTTPACTION");
    close();
    return false;
  }
  if (strstr(response, "+HTTPACTION:") != NULL)
  {
    ESP_LOGI(TAG, "HTTPACTION response: %s", response);
    char *action_start = strstr(response, "+HTTPACTION:");
    if (action_start)
    {
      if (sscanf(action_start, "+HTTPACTION: %d,%d,%d", &method, &status_code, &data_len) == 3)
      {
        ESP_LOGI(TAG, "Parsed HTTPACTION: method=%d, status=%d, length=%d", method, status_code, data_len);
      }
      else
      {
        ESP_LOGE(TAG, "Failed to parse HTTPACTION response: %s", response);
        close();
        return false;
      }
    }
  }
  else
  {
    ESP_LOGE(TAG, "Expected +HTTPACTION not found in response: %s", response);
    close();
    return false;
  }
  // if (status_code != 200)
  // {
  //   ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
  //   close();
  //   return false;
  // }
  ESP_LOGI(TAG, "HTTP request succeeded, data length: %d", data_len);
  // 读取 HTTP 响应
  if (data_len > 0)
  {
    char read_command[64];
    snprintf(read_command, sizeof(read_command), "AT+HTTPREAD=0,%d", data_len);
    if (!at_send_command(read_command, "OK", 13000, response, false))
    {
      ESP_LOGE(TAG, "Failed to read HTTP response");
      close();
      return false;
    }
    ESP_LOGI(TAG, "HTTP response: %s", response);
    // 处理http get 返回
  }
  else
  {
    ESP_LOGE(TAG, "No data available to read");
    close();
    return false;
  }
  close();
  return true;
}