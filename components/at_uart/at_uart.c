#include <dirent.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "at_config.h"
#include "at_utils.h"
#include <freertos/semphr.h>
#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_17 // UART1 TX 引脚
#define RXD_PIN GPIO_NUM_18 // UART1 RX 引脚
#define UART_BUF_LISTEN_SIZE 512

static const char *TAG = "UART";
static SemaphoreHandle_t xMutex = NULL;
static QueueHandle_t messageQueue = NULL;
static bool inited = false;

// 封装互斥锁获取逻辑，增加超时保护
static bool take_mutex_with_timeout(SemaphoreHandle_t mutex, int timeout_ms)
{
    return xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

// 初始化 UART
void at_uart_init()
{
    if (inited)
    {
        ESP_LOGW(TAG, "UART already initialized");
        return;
    }

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    if (uart_driver_install(UART_NUM, UART_BUF_SIZE, 0, 0, NULL, 0) != ESP_OK ||
        uart_param_config(UART_NUM, &uart_config) != ESP_OK ||
        uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        ESP_LOGE(TAG, "UART initialization failed");
        return;
    }

    xMutex = xSemaphoreCreateMutex();
    if (xMutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        uart_driver_delete(UART_NUM);
        return;
    }

    messageQueue = xQueueCreate(10, UART_BUF_LISTEN_SIZE);
    if (messageQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create message queue");
        vSemaphoreDelete(xMutex);
        uart_driver_delete(UART_NUM);
        return;
    }

    inited = true;
    ESP_LOGI(TAG, "UART initialized successfully");
}

// 发送 AT 指令并等待响应
bool at_send_command(const char *command, const char *expected_response, int timeout_ms, char *out_response, bool noR)
{
    if (!inited)
    {
        ESP_LOGE(TAG, "UART not initialized");
        return false;
    }

    if (!take_mutex_with_timeout(xMutex, 500))
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return false;
    }

    char response[UART_BUF_SIZE] = {0};
    uart_flush(UART_NUM);
    uart_write_bytes(UART_NUM, command, strlen(command));
    if (!noR)
    {
        uart_write_bytes(UART_NUM, "\r", 1);
    }

    int elapsed_time = 0;
    while (elapsed_time < timeout_ms)
    {
        int length = uart_read_bytes(UART_NUM, (uint8_t *)response, sizeof(response) - 1, pdMS_TO_TICKS(100));
        if (length > 0)
        {
            response[length] = '\0';
            if (strstr(response, expected_response))
            {
                if (out_response)
                {
                    strncpy(out_response, response, UART_BUF_SIZE - 1);
                    out_response[UART_BUF_SIZE - 1] = '\0';
                }
                xSemaphoreGive(xMutex);
                return true;
            }
        }
        elapsed_time += 100;
    }

    ESP_LOGE(TAG, "Timeout waiting for response. Last response: %s", response);
    if (out_response)
    {
        strncpy(out_response, "ERROR!", UART_BUF_SIZE - 1);
        out_response[UART_BUF_SIZE - 1] = '\0';
    }

    xSemaphoreGive(xMutex);
    return false;
}

// UART 监听任务
void at_uart_listening()
{
    if (!inited || xMutex == NULL || messageQueue == NULL)
    {
        ESP_LOGE(TAG, "UART not properly initialized");
        return;
    }

    char response[UART_BUF_LISTEN_SIZE] = {0};

    while (1)
    {
        if (take_mutex_with_timeout(xMutex, 500))
        {
            int length = uart_read_bytes(UART_NUM, (uint8_t *)response, sizeof(response) - 1, pdMS_TO_TICKS(100));
            if (length > 0)
            {
                response[length] = '\0';
                if (strstr(response, "+MSUB:"))
                {
                    if (xQueueSend(messageQueue, response, pdMS_TO_TICKS(100)) != pdPASS)
                    {
                        ESP_LOGE(TAG, "Failed to send message to queue");
                    }
                }
            }
            xSemaphoreGive(xMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 消息处理任务
void message_handler_task()
{
    char receivedMessage[UART_BUF_LISTEN_SIZE] = {0};

    while (1)
    {
        if (xQueueReceive(messageQueue, receivedMessage, portMAX_DELAY) == pdTRUE)
        {
            char res[UART_BUF_SIZE];
            parse_json(receivedMessage, res);
            ESP_LOGI(TAG, "Processing message: %s", res);
        }
    }
}

// 反初始化 UART
void at_uart_deinit()
{
    if (!inited)
    {
        ESP_LOGW(TAG, "UART not initialized, nothing to deinitialize");
        return;
    }

    vSemaphoreDelete(xMutex);
    vQueueDelete(messageQueue);
    uart_driver_delete(UART_NUM);
    inited = false;

    ESP_LOGI(TAG, "UART deinitialized successfully");
}

bool is_uart_inited()
{
    return inited;
}