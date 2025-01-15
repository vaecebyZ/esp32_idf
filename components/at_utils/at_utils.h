
#ifndef AT_UTILS_H
#define AT_UTILS_H
#include <stdint.h>

void generate_random_uuid(char *uuid, size_t size);
bool initSysTimeByAT();
uint64_t get_current_timestamp_ms();
void parse_json(const char *response);
#endif