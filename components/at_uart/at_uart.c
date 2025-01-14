#include <dirent.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include <stddef.h>
#include "esp_err.h"
#include "driver/gpio.h"

static const char *TAG = "UART";

#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_17 // UART1 TX 引脚
#define RXD_PIN GPIO_NUM_18 // UART1 RX 引脚
#define UART_BUF_SIZE 256

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
        .source_clk = UART_SCLK_APB,
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    // Set UART pins(TX, RX, RTS, CTS)
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "inited");
    inited = true;
}

// Send an AT command and wait for a response
bool at_send_command(const char *command, const char *expected_response, int timeout_ms, char *out_response, bool noR)
{
    char response[UART_BUF_SIZE] = {0};
    uart_flush(UART_NUM);
    ESP_LOGI(TAG, "Sending [%s] expected response: %s", command, expected_response ? expected_response : "NULL");

    uart_write_bytes(UART_NUM, command, strlen(command));
    if (!noR)
    {
        uart_write_bytes(UART_NUM, "\r", 1);
    }

    // Wait for response
    int length = 0;
    int elapsed_time = 0;

    while (elapsed_time < timeout_ms)
    {
        length = uart_read_bytes(UART_NUM, (uint8_t *)response, sizeof(response) - 1, pdMS_TO_TICKS(100));
        if (length > 0)
        {
            response[length] = '\0'; // Null-terminate response
            if (strstr(response, expected_response))
            {
                ESP_LOGI(TAG, "Received expected response : %s", response);

                // If out_response is not NULL, copy the response to the output buffer
                if (out_response != NULL)
                {
                    strncpy(out_response, response, UART_BUF_SIZE - 1);
                    out_response[UART_BUF_SIZE - 1] = '\0'; // Ensure null-termination
                }
                else
                {
                    ESP_LOGW(TAG, "out_response is NULL. Skipping response copy.");
                }

                return true;
            }
        }
        elapsed_time += 100;
    }

    ESP_LOGE(TAG, "Timeout waiting for response. Last response: %s", response);

    // If out_response is not NULL, copy the last received response
    if (out_response != NULL)
    {
        strncpy(out_response, "ERROR!", UART_BUF_SIZE - 1);
        out_response[UART_BUF_SIZE - 1] = '\0'; // Ensure null-termination
    }
    return false;
}

// Deinitialize UART
void at_uart_deinit()
{
    if (inited)
    {
        uart_driver_delete(UART_NUM); // Uninstall UART driver
        ESP_LOGI(TAG, "UART deinitialized");
        inited = false;
    }
    else
    {
        ESP_LOGW(TAG, "UART not initialized, nothing to deinitialize");
    }
}

bool is_uart_inited()
{
    return inited;
}
