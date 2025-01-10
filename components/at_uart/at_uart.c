#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "USAT";

#define UART_NUM UART_NUM_1
#define UART_TX_PIN 17
#define UART_RX_PIN 18
#define UART_BUF_SIZE 1024
#define portTICK_RATE_MS 10


static bool inited = false;

// Initialize UART for AT communication
void at_uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "inited");
    inited = true;
}

bool is_uart_inited()
{
    return inited;
}

void at_uart_send(const char *data)
{
    if (!inited)
    {
        ESP_LOGE(TAG, "UART not inited");
        return;
    }
    uart_write_bytes(UART_NUM, data, strlen(data));
}

int at_uart_read(char *data)
{
    if (!inited)
    {
        ESP_LOGE(TAG, "UART not inited");
        return -1;
    }
    return uart_read_bytes(UART_NUM, (uint8_t *)data, sizeof(data) - 1, 100 / pdMS_TO_TICKS(100));
}