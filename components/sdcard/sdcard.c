#include <string.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#define MOUNT_POINT "/sdcard"
#define FILEPATH_MAX 2048
#define MAX_FILES 100       // 最大文件数量
#define MAX_PATH_LENGTH 300 // 单个路径的最大长度

static const char *TAG = "SD_CARD";

// 文件路径数组结构体
typedef struct FilePathArray
{
    char file_paths[MAX_FILES][MAX_PATH_LENGTH];
    int file_count;
} FilePathArray;

static bool isInited = false;

// 声明一个全局文件路径数组
FilePathArray file_array; // 这里不加 static，使得其他文件可以引用

// 初始化SD卡并列出文件
void list_files_in_directory(const char *base_path, const char *relative_path)
{
    char current_path[MAX_PATH_LENGTH];
    snprintf(current_path, sizeof(current_path), "%s/%s", base_path, relative_path);

    DIR *dir = opendir(current_path);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", current_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // 忽略 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        // 忽略 "System Volume Information" 文件夹和其内容
        if (strcmp(entry->d_name, "System Volume Information") == 0)
        {
            ESP_LOGI(TAG, "Ignored folder: %s", entry->d_name);
            continue;
        }

        char full_relative_path[MAX_PATH_LENGTH];
        if (strlen(relative_path) == 0)
        {
            snprintf(full_relative_path, sizeof(full_relative_path), "/%s", entry->d_name);
        }
        else
        {
            snprintf(full_relative_path, sizeof(full_relative_path), "/%s/%s", relative_path, entry->d_name);
        }

        // 打印当前文件或目录的完整路径
        if (entry->d_type == DT_DIR)
        {
            ESP_LOGI(TAG, "Found directory: %s", full_relative_path);
            // 递归进入子目录
            list_files_in_directory(base_path, full_relative_path + 1);
        }
        else
        {
            // 存储文件路径到数组中
            if (file_array.file_count < MAX_FILES)
            {
                snprintf(file_array.file_paths[file_array.file_count], MAX_PATH_LENGTH, "%s", full_relative_path);
                file_array.file_count++;
                ESP_LOGI(TAG, "Added file: %s", full_relative_path);
            }
            else
            {
                ESP_LOGW(TAG, "File path storage limit reached (%d files).", MAX_FILES);
            }
        }
    }
    closedir(dir);
}

// 初始化SD卡并列出文件
void init_sd_card()
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // 管脚启用内部上拉
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card));
    ESP_LOGI(TAG, "SD card mounted at %s", mount_point);
    sdmmc_card_print_info(stdout, card);

    // 重置文件路径数组
    memset(&file_array, 0, sizeof(file_array));

    // 列出SD卡上的文件
    list_files_in_directory(MOUNT_POINT, "");

    // 打印所有文件路径
    for (int i = 0; i < file_array.file_count; i++)
    {
        ESP_LOGI(TAG, "File %d: %s", i, file_array.file_paths[i]);
    }
    isInited = true;
}

bool sdcard_is_inited()
{
    return isInited;
}