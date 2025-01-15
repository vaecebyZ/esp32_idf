#include <stdio.h>
#include "esp_timer.h"
#include <stdint.h>

uint64_t get_current_timestamp_ms() {
    return esp_timer_get_time() / 1000; // 转换为毫秒
}