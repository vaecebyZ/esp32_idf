// UART.h
#ifndef AT_UART_H
#define AT_UART_H
#include <stdbool.h>
void at_uart_init();

void at_uart_deinit();

bool is_uart_inited();
// bool at_send_hex(unsigned char *command, char *expected_response, int timeout_ms, bool flag);

bool at_send_command(char *command, char *expected_response, int timeout_ms, char *out_response,bool noR);
#endif