#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_err.h"
#include "led.h"
#include "wifi.h"
#include "sdcard.h"
#include "server.h"

static const char *TAG = "HTTP_SD_SERVER";
// 在 main.c 中通过 extern 引用 sdcard.c 中的 file_array
extern FilePathArray file_array;

void app_main()
{
    ESP_LOGW(TAG, "Starting app_main()...");
    init_gpio();
    set_led_on();

    // 初始化 Wi-Fi
    ESP_LOGW(TAG, "Initializing Wi-Fi...");
    wifi_init_sta();
    while (!wifi_is_connected())
    {
        ESP_LOGW(TAG, "Waiting for Wi-Fi connection...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "Initializing SDCard...");
    init_sd_card();
    while (!sdcard_is_inited())
    {
        ESP_LOGI(TAG, "Waiting for Card Iint...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "Initializing HTTPD...");

    start_http_server();

    while (!httpd_is_inited())
    {
        ESP_LOGI(TAG, "Waiting for HTTPD Iint...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    set_led_off();
}
