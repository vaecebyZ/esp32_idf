
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "led.h"

#define WIFI_AP_SSID "ESP32_Config"
#define WIFI_AP_PASS "12345678"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONN 4
#define POST_BUFFER_SIZE 128 // 设置缓冲区大小
#define MAX_TIMEOUT 5        // sta模式下wifi重连接次数

static const char *TAG = "WIFI_ADAPTER";

static bool isInited = false;

// 初始化spiffs
void init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",        // 挂载点
        .partition_label = "storage",  // 分区名称
        .max_files = 5,                // 最大打开文件数
        .format_if_mount_failed = true // 如果挂载失败是否格式化
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        ESP_LOGE("SPIFFS", "SPIFFS initialization failed");
        return;
    }

    size_t total = 0, used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK)
    {
        ESP_LOGI("SPIFFS", "Total space: %d, Used space: %d", total, used);
    }
    else
    {
        ESP_LOGE("SPIFFS", "Failed to get SPIFFS partition information");
    }
}

// Wi-Fi事件处理
static int timeout = 0;
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "CONNECT to Wi-Fi...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Successfully connected to Wi-Fi.");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reconnecting to Wi-Fi...");
        if (timeout > MAX_TIMEOUT)
        {
            // 超过最大超时时间，清除 NVS 中的 Wi-Fi 配置
            ESP_LOGI(TAG, "Maximum reconnection attempts reached. Clearing Wi-Fi NVS data and switching to AP mode.");

            for (int i = 0; i < 6; i++)
            {
                if (i % 2 == 0)
                {
                    set_led_off();
                }
                else
                {
                    set_led_on();
                }
                vTaskDelay(pdMS_TO_TICKS(500));
            }

            // 清除 Wi-Fi 配置
            esp_wifi_stop();
            esp_wifi_deinit();
            nvs_flash_erase(); // 清除 NVS 中的数据
            esp_restart();     // 重启
        }
        else
        {
            timeout++;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip));
        isInited = true;
    }
}

// 保存wifi配置到nvs
static esp_err_t save_wifi_config_to_nvs(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // 提交更改
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit changes: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

// 从nvs中加载wifi配置
static esp_err_t load_wifi_config_from_nvs(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // 获取 SSID
    err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "SSID not found in NVS");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // 获取 Password
    err = nvs_get_str(nvs_handle, "password", password, &password_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Password not found in NVS");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get password: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NVS_NOT_FOUND : ESP_OK;
}

// 解码
static void url_decode(char *dest, const char *src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dest++ = 16 * a + b;
            src += 3;
        }
        else if (*src == '+')
        {
            *dest++ = ' ';
            src++;
        }
        else
        {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

// 提交句柄处理
static esp_err_t wifi_config_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Handler");
    char buf[POST_BUFFER_SIZE]; // 使用更大的缓冲区
    int len = httpd_req_recv(req, buf, sizeof(buf));
    if (len <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    buf[len] = '\0';

    char ssid[256] = {0};
    char password[256] = {0};

    // 解码收到的POST数据
    char decoded_buf[POST_BUFFER_SIZE];
    url_decode(decoded_buf, buf);

    // 查找ssid和password的键值
    char *ssid_start = strstr(decoded_buf, "ssid=");
    char *password_start = strstr(decoded_buf, "&password=");

    if (ssid_start && password_start)
    {
        ssid_start += strlen("ssid=");
        password_start += strlen("&password=");

        size_t ssid_len = password_start - ssid_start - strlen("&password=");
        strncpy(ssid, ssid_start, ssid_len);
        ssid[ssid_len] = '\0';

        strncpy(password, password_start, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
    }
    else
    {
        ESP_LOGE(TAG, "Failed to parse SSID or password");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received SSID: %s, Password: %s", ssid, password);

    // 保存账号和密码
    // 保存到 NVS
    esp_err_t err = save_wifi_config_to_nvs(ssid, password);
    if (err != ESP_OK)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Configuration saved. Rebooting...");

    esp_restart();

    return ESP_OK;
}

// 获取配置页面句柄
static esp_err_t wifi_config_get_handler(httpd_req_t *req)
{
    FILE *file = fopen("/spiffs/index.html", "r");
    if (!file)
    {
        ESP_LOGE("SPIFFS", "Failed to open file");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char buf[128];
    size_t read_bytes;

    httpd_resp_set_type(req, "text/html");

    while ((read_bytes = fread(buf, 1, sizeof(buf), file)) > 0)
    {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); // 结束响应
    return ESP_OK;
}

// 提交配置
static httpd_uri_t wifi_config_post = {
    .uri = "/wifi-config",
    .method = HTTP_POST,
    .handler = wifi_config_post_handler,
};

// 获取配置
static httpd_uri_t wifi_config_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = wifi_config_get_handler,
};

// 启用webserver
static httpd_handle_t start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;      // 增大堆栈大小（根据需要调整）
    config.recv_wait_timeout = 10; // 增加接收超时时间
    config.send_wait_timeout = 10; // 增加发送超时时间
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &wifi_config_get);
        httpd_register_uri_handler(server, &wifi_config_post);
    }

    return server;
}

// 启用ap模式
static void start_ap_mode()
{
    ESP_LOGI(TAG, "Starting AP mode");

    // 初始化TCP/IP适配器
    esp_netif_init();

    // 创建默认的WIFI AP网络接口
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();

    // 配置AP静态IP地址 (可选，默认ESP32是192.168.4.1)
    esp_netif_ip_info_t ip_info;
    esp_netif_dhcps_stop(netif); // 先停止DHCP服务以配置静态IP
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1); // 网关地址
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_set_ip_info(netif, &ip_info);
    esp_netif_dhcps_start(netif); // 重新启动DHCP服务

    // 初始化WIFI配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASS,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_AP_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started. SSID:%s, Password:%s", WIFI_AP_SSID, WIFI_AP_PASS);

    // 启动Web服务器
    start_webserver();
}

// 启用sta模式
static void start_sta_mode(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Starting STA mode");
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_err_t err;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register Wi-Fi event handler: %s", esp_err_to_name(err));
    }
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_event_handler, NULL, &instance_any_id);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "%s", ssid);
    ESP_LOGI(TAG, "%s", password);
    ESP_LOGI(TAG, "SSID length: %d, Password length: %d", strlen(ssid), strlen(password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// 初始化Wi-Fi
void wifi_init()
{

    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    char ssid[64] = {0};
    char password[64] = {0};

    if (load_wifi_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK)
    {
        // 如果有配置，尝试连接
        ESP_LOGI(TAG, "Loaded Wi-Fi config: SSID: %s", ssid);
        start_sta_mode(ssid, password);
    }
    else
    {
        // 如果没有配置，启动AP模式
        init_spiffs();
        start_ap_mode();
    }
}

// 是否已经连接wifi
bool wifi_is_connected()
{
    return isInited;
}