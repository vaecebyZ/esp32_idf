#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

void init_sd_card(void) {
    esp_err_t ret;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Options for mounting the filesystem.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
    };

    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("SD_CARD", "Failed to mount filesystem. Error: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("SD_CARD", "SD card mounted successfully.");
}
