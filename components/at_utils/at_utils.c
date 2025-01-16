#include <stdio.h>
#include "esp_random.h"
#include <inttypes.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "at_uart.h"
#include "at_check.h"
#include "at_config.h"
#include <string.h>
static const char *TAG = "AT_UTILS";

static bool isTimeSet = false;

void set_timezone(const char *timezone)
{
    setenv("TZ", timezone, 1);
    tzset();
}

void set_esp32_time(int year, int month, int day, int hour, int minute, int second)
{
    set_timezone("CST-8");
    struct tm timeinfo = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second,
    };

    time_t t = mktime(&timeinfo);
    struct timeval now = {
        .tv_sec = t,
        .tv_usec = 0,
    };
    settimeofday(&now, NULL);
    isTimeSet = true;
}

bool initSysTimeByAT()
{
    if (at_check_base() && at_check_pdp())
    {
        char response[UART_BUF_SIZE];
        at_send_command("AT+CCLK?", "OK", 1000, response, false);
        ESP_LOGI(TAG, "AT+CCLK? response: %s", response);
        if (strlen(response) == 0)
        {
            ESP_LOGE(TAG, "Empty response received for AT+CCLK");
            return false;
        }
        if (strstr(response, "+CCLK:") != NULL)
        {
            char *action_start = strstr(response, "+CCLK:");
            if (action_start)
            {
                char cclk_line[64] = {0};
                // 提取 +CCLK: 行，仅保留相关内容
                sscanf(action_start, "+CCLK: \"%63[^\"]\"", cclk_line);
                int year, month, day, hour, minute, second;
                ESP_LOGW(TAG, "cclk_line: %s", cclk_line);
                if (sscanf(cclk_line, "%d/%d/%d,%d:%d:%d",
                           &year, &month, &day, &hour, &minute, &second) == 6)
                {
                    year += 2000; // Adjust year
                    set_esp32_time(year, month, day, hour, minute, second);
                    ESP_LOGI(TAG, "Set system time to %d/%d/%d %d:%d:%d", year, month, day, hour, minute, second);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to parse CCLK response: %s", response);
                    return false;
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "Expected CCLK not found in response: %s", response);
            return false;
        }
    }
    return true;
}

uint64_t get_current_timestamp_ms()
{
    if (isTimeSet)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
    }
    else
    {
        return 0;
    }
}

void generate_random_uuid(char *uuid, size_t size)
{
    uint32_t rand1 = esp_random();
    uint32_t rand2 = esp_random();
    uint32_t rand3 = esp_random();
    uint32_t rand4 = esp_random();

    snprintf(uuid, size,
             "%08" PRIX32 "-%04" PRIX32 "-%04" PRIX32 "-%04" PRIX32 "-%04" PRIX32 "%08" PRIX32,
             rand1,                             // 时间戳或随机数
             (rand2 & 0xFFFF),                  // 时间戳或随机数
             ((rand2 >> 16) & 0x0FFF) | 0x4000, // 版本号 (UUID v4)
             (rand3 & 0x3FFF) | 0x8000,         // Variant (UUID v4)
             (rand3 >> 16),                     // 随机数
             rand4                              // 随机数
    );
}

void parse_json(const char *input, char *output)
{
    const char *start = strchr(input, '{'); // 找到第一个 '{'
    const char *end = strrchr(input, '}');  // 找到最后一个 '}'

    if (start && end && end > start)
    {
        size_t length = end - start + 1; // 计算 JSON 的长度
        strncpy(output, start, length);  // 复制 JSON 数据
        output[length] = '\0';           // 添加字符串结束符
    }
    else
    {
        strcpy(output, "{}"); // 如果没找到，返回错误信息
    }
}