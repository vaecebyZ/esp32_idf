
#ifndef AT_MQ_H
#define AT_MQ_H
bool at_mq_connect(const char *path);
bool at_mq_publish();
bool at_mq_subscribe();
bool at_mq_free();
#endif