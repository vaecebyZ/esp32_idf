#include "at_check.h"
#include "at_uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "at_config.h"
#include "at_utils.h"

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

void at_mq_subscribe(const char *topic)
{
  if (topic == NULL)
  {
    ESP_LOGE(TAG, "topic is NULL");
    return;
  }
  char command[UART_BUF_SIZE];
  // 订阅topic
  sprintf(command, "AT+MSUB=\"%s\",0", topic);
  if (!at_send_command(command, "SUBACK", 3000, NULL, false))
  {
    ESP_LOGE(TAG, "AT+MSUB failed");
    return;
  }
  ESP_LOGW(TAG, "Subscribed to %s", topic);
  return;
}

bool at_mq_publish(const mqMessage_t mqMessage, char *expected_response, char *responseJSON)
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
  if (expected_response == NULL)
  {
    expected_response = "+MSUB:";
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
  cJSON_Delete(root);
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
  char response[UART_BUF_SIZE];
  if (!at_send_command(send_command, expected_response, 5000, response, true))
  {
    ESP_LOGE(TAG, "AT+MPUBX send failed");
    free(send_command);
    return false;
  }
  free(send_command);
  free(payload);
  if (responseJSON != NULL)
  {
    parse_json(response, responseJSON);
  }
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

bool getHeartbeatResponse(const char *res)
{
  //  {
  //     "id": "52D7BD9D-42A1-4DB6-BFE4-59FAB83995BF",
  //     "data": {
  //         "time": 1737008394298,
  //         "Status": 1,
  //         "Msg": "Success",
  //         "message": ""
  //     },
  //     "time": 1737008394298,
  //     "ttl": 5000,
  //     "event": "ping"
  // }
  cJSON *root = cJSON_Parse(res);
  if (root == NULL)
  {
    ESP_LOGE(TAG, "Failed to parse JSON");
    return false;
  }

  cJSON *data = cJSON_GetObjectItem(root, "data");

  if (data == NULL)
  {
    ESP_LOGE(TAG, "Failed to get data");
    cJSON_Delete(root);
    return false;
  }
  cJSON *code = cJSON_GetObjectItem(data, "Status");
  if (code == NULL)
  {
    ESP_LOGE(TAG, "Failed to get code");
    cJSON_Delete(root);
    return false;
  }
  if (code->valueint != 1)
  {
    ESP_LOGE(TAG, "Failed to get code");
    cJSON_Delete(root);
    return false;
  }
  ESP_LOGI(TAG, "Heartbeat success get code %d", code->valueint);
  cJSON_Delete(root);
  return true;
}

bool at_mq_heartbeat()
{
  char topic[UART_BUF_SIZE];
  sprintf(topic, "/device/%s/ping", mqconfig.clientId);
  ESP_LOGW(TAG, "Heartbeat topic: %s", topic);
  cJSON *payload = cJSON_CreateObject();
  cJSON_AddStringToObject(payload, "deviceId", mqconfig.clientId);
  cJSON_AddStringToObject(payload, "projectInfoCode", "PJ202406050002");
  char uuid[37];
  generate_random_uuid(uuid, sizeof(uuid));
  mqMessage_t heartbeat = {
      .topic = topic,
      .event = Ping,
      .data = payload,
      .time = get_current_timestamp_ms(),
      .ttl = 5000,
      .id = uuid,
  };
  char res[UART_BUF_SIZE];
  if (at_mq_publish(heartbeat, "/ping/reply", res))
  {
    ESP_LOGW(TAG, "Heartbeat response %s", res);

    return getHeartbeatResponse(res);
  }
  return false;
}

void at_mq_heartbeat_task()
{
  char topic[UART_BUF_SIZE];
  sprintf(topic, "/device/%s/ping/#", mqconfig.clientId);
  at_mq_subscribe(topic);
  while (1)
  {
    if (!at_mq_heartbeat())
    {
      ESP_LOGE(TAG, "Heartbeat failed");
      vTaskDelete(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

bool at_mq_listening()
{
  xTaskCreatePinnedToCore(at_mq_heartbeat_task, "at_mq_heartbeat_task", 4096, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(at_uart_listening, "at_uart_listening", 4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(message_handler_task, "message_handler_task", 4096, NULL, 5, NULL, 1);
  // 监听消息
  return true;
}