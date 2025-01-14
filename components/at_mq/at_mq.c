#include "at_check.h"
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "cJSON.h"

#define UART_BUF_SIZE 256

typedef struct
{
  char *clientId;
  char *username;
  char *password;
} MqConfig;

static MqConfig config = {
    .clientId = "",
    .username = "",
    .password = "",
};

static bool close()
{
  // AT+MIPCLOSE
  if (!at_send_command("AT+MDISCONNECT", "OK", 1000, NULL, false))
  {
    ESP_LOGE("MQ", "AT+MDISCONNECT failed");
    return false;
  }

  if (!at_send_command("AT+MIPCLOSE", "OK", 1000, NULL, false))
  {
    ESP_LOGE("MQ", "AT+MIPCLOSE failed");
    return false;
  }

  return true;
}

bool at_mq_free()
{
  return close();
}

// bool at_mq_subscribe(const char *topic)
bool at_mq_subscribe()

{
  char command[UART_BUF_SIZE];
  // 订阅topic
  // /platform/kbmqvsoj/regist
  char *topic = "/platform/kbmqvsoj/regist/#";

  sprintf(command, "AT+MSUB=\"%s\",0", topic);
  if (!at_send_command(command, "SUBACK", 3000, NULL, false))
  {
    ESP_LOGE("MQ", "AT+MSUB failed");
    return false;
  }
  free(command);
  return true;
}

// bool at_mq_publish(const char *topic, const char *payload)
bool at_mq_publish()
{
  char command[UART_BUF_SIZE];
  // 发送消息
  cJSON *root = cJSON_CreateObject();
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "deviceId", config.clientId);
  cJSON_AddStringToObject(data, "deviceName", "ESP32_AT");
  cJSON_AddStringToObject(data, "deviceCate", "Elevator");
  cJSON_AddStringToObject(data, "mqttUserName", config.username);
  cJSON_AddStringToObject(data, "projectInfoCode", "PJ202406050002"); // 以后不用带
  cJSON_AddItemToObject(root, "data", data);
  cJSON_AddStringToObject(root, "event", "registDevice");
  cJSON_AddStringToObject(root, "id", "");
  cJSON_AddStringToObject(root, "messageType", "event");
  cJSON_AddNumberToObject(root, "time", 123);
  cJSON_AddNumberToObject(root, "ttl", 5000);
  char *payload = cJSON_PrintUnformatted(root);
  char *topcpub = "/platform/kbmqvsoj/regist";
  // 发送长数据
  sprintf(command, "AT+MPUBEX=\"%s\",0,0,%d", topcpub, strlen(payload) + 1);
  if (!at_send_command(command, ">", 1000, NULL, false))
  {
    ESP_LOGE("MQ", "AT+MPUB failed");
    return false;
  }

  char *send_command = NULL;
  size_t len = snprintf(NULL, 0, "%s", payload) + 1;
  send_command = (char *)malloc(len);
  if (send_command == NULL)
  {
    // 处理分配失败的情况
    ESP_LOGE("MQ", "malloc failed");
    return false;
  }

  sprintf(send_command, "%s", payload);
  if (!at_send_command(send_command, "+MSUB:", 5000, NULL, true))
  {
    ESP_LOGE("MQ", "AT+MPUBX send failed");
    free(send_command);
    return false;
  }
  free(send_command);
  free(payload);
  return true;
}

bool at_mq_connect(const char *path)
{
  if (path == NULL || strlen(path) == 0)
  {
    ESP_LOGE("HTTP", "Invalid path");
    return false;
  }
  char response[UART_BUF_SIZE];
  // 基本检查
  if (!at_check_base())
    return false;
  // PDP 检查
  if (!at_check_pdp())
    return false;

  char *clientId = config.clientId;
  char *username = config.username;
  char *password = config.clientId;
  // 设置MQTT参数客户端ID，用户名，密码，遗嘱一般不设置
  char command[UART_BUF_SIZE];
  sprintf(command, "AT+MCONFIG=%s,%s,%s", clientId, username, password);
  if (!at_send_command(command, "OK", 1000, response, false))
  {
    ESP_LOGE("MQ", "AT+MCONFIG failed");
    return false;
  }

  // 连接MQTT服务器,设置服务器地址和端口
  char *server = "";
  char *port = "";
  sprintf(command, "AT+MIPSTART=%s,%s", server, port);

  // 查看是否已经连接
  if (!at_send_command(command, "CONNECT OK", 3000, NULL, false))
  {
    // 新连接
    if (!at_send_command(command, "ALREADY CONNECT", 3000, NULL, false))
    {
      ESP_LOGE("MQ", "AT+MIPSTART failed");
      return false;
    }
  }
  // 发起会话
  // AT+MCONNECT=1,120
  if (!at_send_command("AT+MCONNECT=1,120", "CONNACK OK", 3000, NULL, false))
  {
    ESP_LOGE("MQ", "AT+MCONNECT failed");
    at_check_reset();
    esp_restart();
    return false;
  }

  return true;
}