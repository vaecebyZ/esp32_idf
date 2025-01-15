#include "at_check.h"
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "cJSON.h"
#include "at_config.h"

#define UART_BUF_SIZE 256

static char *TAG = "MQ";

static mqConfig_t mqconfig = {
    .username = NULL,
    .password = NULL,
    .server = NULL,
    .port = NULL,
};

static bool close()
{
  // AT+MIPCLOSE
  if (!at_send_command("AT+MDISCONNECT", "OK", 1000, NULL, false))
  {
    ESP_LOGE(TAG, "AT+MDISCONNECT failed");
    return false;
  }

  if (!at_send_command("AT+MIPCLOSE", "OK", 1000, NULL, false))
  {
    ESP_LOGE(TAG, "AT+MIPCLOSE failed");
    return false;
  }

  return true;
}

bool at_mq_free()
{
  return close();
}

bool at_mq_subscribe(const char *topic)
{
  if (topic == NULL)
  {
    ESP_LOGE(TAG, "topic is NULL");
    return false;
  }
  char command[UART_BUF_SIZE];
  // 订阅topic
  sprintf(command, "AT+MSUB=\"%s\",0", topic);
  if (!at_send_command(command, "SUBACK", 3000, NULL, false))
  {
    ESP_LOGE(TAG, "AT+MSUB failed");
    return false;
  }
  return true;
}

bool at_mq_publish(mqMessage_t mqMessage)
{
  if (!validateMqConfig(&mqconfig))
  {
    ESP_LOGE(TAG, "Invalid mqconfig");
    return false;
  }
  if (!validateMqMessage(&mqMessage))
  {
    ESP_LOGE(TAG, "Invalid mqMessage");
    return false;
  }
  char command[UART_BUF_SIZE];
  // 发送消息
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", mqMessage.data);
  cJSON_AddStringToObject(root, "event", getEventString(mqMessage.event));
  cJSON_AddStringToObject(root, "id", mqMessage.id);
  cJSON_AddNumberToObject(root, "time", mqMessage.time);
  cJSON_AddNumberToObject(root, "ttl", mqMessage.ttl);
  char *payload = cJSON_PrintUnformatted(root);
  char *topcpub = mqMessage.topic;
  // 发送长数据
  sprintf(command, "AT+MPUBEX=\"%s\",0,0,%d", topcpub, strlen(payload) + 1);
  if (!at_send_command(command, ">", 1000, NULL, false))
  {
    ESP_LOGE(TAG, "AT+MPUB failed");
    return false;
  }

  char *send_command = NULL;
  size_t len = snprintf(NULL, 0, "%s", payload) + 1;
  send_command = (char *)malloc(len);
  if (send_command == NULL)
  {
    // 处理分配失败的情况
    ESP_LOGE(TAG, "malloc failed");
    return false;
  }

  sprintf(send_command, "%s", payload);
  if (!at_send_command(send_command, "+MSUB:", 5000, NULL, true))
  {
    ESP_LOGE(TAG, "AT+MPUBX send failed");
    free(send_command);
    return false;
  }
  free(send_command);
  free(payload);
  return true;
}

bool at_mq_connect(const mqConfig_t config)
{
  if (!validateMqConfig(&config))
  {
    ESP_LOGE(TAG, "Invalid config");
    return false;
  }
  else
  {
    mqconfig = config;
  }
  char response[UART_BUF_SIZE];
  // 基本检查
  if (!at_check_base())
    return false;
  // PDP 检查
  if (!at_check_pdp())
    return false;

  // 设置MQTT参数客户端ID，用户名，密码，遗嘱一般不设置
  char command[UART_BUF_SIZE];
  sprintf(command, "AT+MCONFIG=%s,%s,%s", config.clientId, config.username, config.password);
  if (!at_send_command(command, "OK", 1000, response, false))
  {
    ESP_LOGE(TAG, "AT+MCONFIG failed");
    return false;
  }

  // 连接MQTT服务器,设置服务器地址和端口
  sprintf(command, "AT+MIPSTART=%s,%s", config.server, config.port);
  // 查看是否已经连接
  if (!at_send_command(command, "CONNECT OK", 3000, NULL, false))
  {
    // 新连接
    if (!at_send_command(command, "ALREADY CONNECT", 3000, NULL, false))
    {
      ESP_LOGE(TAG, "AT+MIPSTART failed");
      return false;
    }
  }
  // 发起会话
  // AT+MCONNECT=1,120
  if (!at_send_command("AT+MCONNECT=1,120", "CONNACK OK", 3000, NULL, false))
  {
    ESP_LOGE(TAG, "AT+MCONNECT failed");
    at_check_reset();
    esp_restart();
    return false;
  }

  return true;
}