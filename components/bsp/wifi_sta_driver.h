#ifndef WIFI_STA_DRIVER_H
#define WIFI_STA_DRIVER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ESP32 WiFi in Station mode.
 * 
 * This function initializes the ESP32 WiFi driver, configures it in station mode,
 * and connects to the specified WiFi network.
 */
void wifi_init_sta(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STA_DRIVER_H