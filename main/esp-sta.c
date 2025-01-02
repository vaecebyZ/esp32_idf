#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"

#define WIFI_SSID "FYX_6E6F_2.4G"
#define WIFI_PASS "zswlzswl"
#define MOUNT_POINT "/sdcard"
#define LED_GPIO GPIO_NUM_27
#define ON 1
#define OFF 0
#define MAX_URI_LENGTH 254 // 根据实际需求调整
#define FILEPATH_MAX 1024
#define MAX_FILES 100       // 最大文件数量
#define MAX_PATH_LENGTH 300 // 单个路径的最大长度

static const char *TAG = "HTTP_SD_SERVER";

// Wi-Fi事件处理
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reconnecting to Wi-Fi...");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip));
    }
}

// 初始化Wi-Fi
void wifi_init_sta()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// 检查路径合法性（防止路径遍历）
static bool is_valid_path(const char *path)
{
    return strstr(path, "..") == NULL;
}

// 根据文件扩展名获取 MIME 类型
static const char *get_mime_type(const char *filepath)
{
    if (strstr(filepath, ".HTM"))
        return "text/html";
    if (strstr(filepath, ".CSS"))
        return "text/css";
    if (strstr(filepath, ".JS"))
        return "application/javascript";
    if (strstr(filepath, ".PNG"))
        return "image/png";
    if (strstr(filepath, ".JPG") || strstr(filepath, ".JPEG"))
        return "image/jpeg";
    if (strstr(filepath, ".GIF"))
        return "image/gif";
    if (strstr(filepath, ".ICO"))
        return "image/x-icon";
    if (strstr(filepath, ".JSON"))
        return "application/json";
    return "text/plain"; // 默认 MIME 类型
}

// 通用文件处理器
esp_err_t file_server_handler(httpd_req_t *req)
{
    char filepath[FILEPATH_MAX];

    // 处理根路径情况，将其映射到默认文件
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0)
    {
        uri = "/INDEX~1.HTM";
    }

    // 拼接文件路径
    snprintf(filepath, FILEPATH_MAX, "%s%s", MOUNT_POINT, uri);

    ESP_LOGI(TAG, "Requested file: %s", filepath);

    // 检查路径合法性
    if (!is_valid_path(uri))
    {
        ESP_LOGE(TAG, "Invalid path: %s", uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
        return ESP_FAIL;
    }

    // 打开文件
    FILE *file = fopen(filepath, "r");
    if (!file)
    {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    // 设置 MIME 类型
    httpd_resp_set_type(req, get_mime_type(filepath));

    // 读取文件并发送
    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK)
        {
            fclose(file);
            ESP_LOGE(TAG, "Failed to send file chunk");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    // 结束响应
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    ESP_LOGI(TAG, "File sent successfully: %s", filepath);
    return ESP_OK;
}

// char file_paths[MAX_FILES][MAX_PATH_LENGTH];
httpd_uri_t basic_handlers[MAX_FILES];
int file_count = 0; // 当前存储的文件数量
// 递归函数：列出目录及其子目录中的所有文件
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
        char full_relative_path[300];
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
            // 忽略特殊目录 "." 和 ".."
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                // 递归进入子目录，但不打印目录名
                list_files_in_directory(base_path, full_relative_path + 1);
            }
        }
        else
        {
            // ESP_LOGI(TAG, "FILE: %s", full_relative_path);
            // 存储文件路径到数组中
            if (file_count < MAX_FILES)
            {
                basic_handlers[file_count].uri = strdup(full_relative_path); // 动态分配内存保存路径
                basic_handlers[file_count].method = HTTP_GET;                // 示例：默认设置为 GET 方法
                basic_handlers[file_count].handler = file_server_handler;    // 示例：将 handler 初始化为 NULL
                basic_handlers[file_count].user_ctx = NULL;                  // 示例：将 user_ctx 初始化为 NULL
                ESP_LOGI(TAG, "Added file: %s", full_relative_path);
                file_count++;
            }
            else
            {
                ESP_LOGW(TAG, "File path storage limit reached (%d files).", MAX_FILES);
            }
        }
    }
    closedir(dir);
}

static const int basic_handlers_no = sizeof(basic_handlers) / sizeof(httpd_uri_t);

// 注册 URI 处理器
esp_err_t register_static_file_server(httpd_handle_t server)
{
    // 循环读出所有路径下的文件并且注册
    int i;
    for (i = 0; i < basic_handlers_no; i++)
    {
        ESP_LOGI(TAG, "URI %d: %s", i, basic_handlers[i].uri);
        esp_err_t res = httpd_register_uri_handler(server, &basic_handlers[i]);

        if (res == ESP_OK)
        {
        }
        else if (res == ESP_ERR_HTTPD_HANDLER_EXISTS)
        {
            ESP_LOGI(TAG, "file EXISTS");
            return ESP_FAIL;
        }
        else if (res == ESP_ERR_HTTPD_HANDLERS_FULL)
        {
            ESP_LOGI(TAG, "ESP_ERR_HTTPD_HANDLERS_FULL");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}
// HTTP 服务器初始化
httpd_handle_t start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 128;
    // 启动 HTTP 服务器
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP server started");
        esp_err_t ret = register_static_file_server(server); // 注册静态资源处理器
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Static file server registered successfully");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to register static file server: %s", esp_err_to_name(ret));
            httpd_stop(server);
            return NULL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }

    return server;
}

// 初始化SD卡
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
    list_files_in_directory(mount_point, "");
}

// 初始化GPIO
void init_gpio()
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1 << LED_GPIO),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
}

void app_main()
{
    init_gpio();
    init_sd_card();
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();
    start_http_server();
}
