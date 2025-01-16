
#ifndef AT_MQ_H
#define AT_MQ_H
#include "at_config.h"
bool at_mq_connect(const mqConfig_t config);
bool at_mq_publish(const mqMessage_t message,char *expected_response,char *responseJSON);
void at_mq_subscribe(const char *topic);
bool at_mq_free();
bool at_mq_listening();
#endif