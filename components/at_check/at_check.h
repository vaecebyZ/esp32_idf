
#ifndef AT_CHECK_H
#define AT_CHECK_H
#include <stdbool.h>
bool at_check_base();
bool at_check_ping();
bool at_check_pdp();
bool at_check_reset();
char *at_get_iccid();
#endif