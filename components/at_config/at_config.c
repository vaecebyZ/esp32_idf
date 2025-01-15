
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "cJSON.h"

// MQ 相关
typedef enum
{
  Consumer,
  GateWay,
  Door,
  GateDoor,
  SmartLock,
  Elevator,
} deviceCateEnum;

char *deviceCateOptionStrings[] = {
    "Consumer",
    "GateWay",
    "Door",
    "GateDoor",
    "SmartLock",
    "Elevator",
};

typedef struct
{
  char *server;
  char *port;
  char *clientId;
  char *username;
  char *password;
} mqConfig_t;

bool validateMqConfig(const mqConfig_t *config)
{
  // 检查指针是否为 NULL
  if (!config)
  {
    return false;
  }
  // 校验每个字段是否为空
  return config->server &&
         config->port &&
         config->clientId &&
         config->username &&
         config->password;
}

char *getDeviceCateString(const deviceCateEnum option)
{
  if (option >= Consumer && option <= Elevator)
  {
    return deviceCateOptionStrings[option];
  }
  return "Invalid Option";
}

typedef enum
{
  Online,
  Ping,
  RegistDevice,
  RegistGateway,
  ServerTime,
  SystemTime,
  Offline,
} eventEnum;

char *eventOptionStrings[] = {
    "online",
    "ping",
    "registDevice",
    "registGateway",
    "ServerTime",
    "systemTime",
    "offline",
};

typedef struct
{
  char *topic;
  char *id;
  cJSON *data;
  eventEnum event;
  uint64_t time;
  uint64_t ttl;
} mqMessage_t;

char *getEventString(const eventEnum option)
{
  if (option >= Online && option <= Offline)
  {
    return eventOptionStrings[option];
  }
  return "Invalid Option";
}

bool validateMqMessage(const mqMessage_t *message)
{
  // 检查指针是否为 NULL
  if (!message)
  {
    return false;
  }
  // 校验每个字段是否为空
  return message->topic &&
         message->id &&
         message->data &&
         message->time &&
         message->ttl;
}
// MQ相关结束