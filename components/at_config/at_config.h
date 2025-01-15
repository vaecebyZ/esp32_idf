#ifndef AT_CONFIG_H
#define AT_CONFIG_H

#define UART_BUF_SIZE 256
#include "cJSON.h"
#include <stdint.h>

typedef enum
{
  Consumer,
  GateWay,
  Door,
  GateDoor,
  SmartLock,
  Elevator,
  NONE,
} deviceCateEnum;

typedef struct
{
  char *server;
  char *port;
  char *clientId;
  char *username;
  char *password;
} mqConfig_t;

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

typedef struct
{
  char *topic;
  char *id;
  cJSON *data;
  eventEnum event;
  uint64_t time;
  uint64_t ttl;
} mqMessage_t;

bool validateMqConfig(const mqConfig_t *config);
bool validateMqMessage(const mqMessage_t *message);
char *getEventString(const eventEnum option);
char *getDeviceCateString(const deviceCateEnum option);

#endif