#include <dirent.h>
#include "esp_http_server.h"
#include "sdcard.h"
#include "esp_log.h"

static const char *TAG = "HTTPD_SERVER";

#define FILEPATH_MAX 1024
#define MAX_FILES 100
#define MOUNT_POINT "/sdcard"

static bool isInited = false;
// 在 main.c 中通过 extern 引用 sdcard.c 中的 file_array
extern FilePathArray file_array;
// 检查路径合法性（防止路径遍历）
static bool is_valid_path(const char *path)
{
  // 这里检查路径中是否包含 ".."，防止路径遍历漏洞
  return strstr(path, "..") == NULL && path[0] == '/';
}

// 根据文件扩展名获取 MIME 类型
static const char *get_mime_type(const char *filepath)
{
  if (strstr(filepath, ".html"))
    return "text/html";
  if (strstr(filepath, ".css"))
    return "text/css";
  if (strstr(filepath, ".js"))
    return "application/javascript";
  if (strstr(filepath, ".png"))
    return "image/png";
  if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg"))
    return "image/jpeg";
  if (strstr(filepath, ".gif"))
    return "image/gif";
  if (strstr(filepath, ".ico"))
    return "image/x-icon";
  if (strstr(filepath, ".json"))
    return "application/json";

  // 处理字体格式
  if (strstr(filepath, ".ttf"))
    return "font/ttf";
  if (strstr(filepath, ".otf"))
    return "font/otf";
  if (strstr(filepath, ".woff"))
    return "font/woff";
  if (strstr(filepath, ".woff2"))
    return "font/woff2";
  if (strstr(filepath, ".eot"))
    return "application/vnd.ms-fontobject";

  return "text/plain"; // 默认 MIME 类型
}

// 通用文件处理器
esp_err_t file_server_handler(httpd_req_t *req)
{

  ESP_LOGI(TAG, "catch %s", req->uri);

  char filepath[FILEPATH_MAX];

  // 处理根路径情况，将其映射到默认文件
  const char *uri = req->uri;
  if (strcmp(uri, "/") == 0)
  {
    uri = "/index.html"; // 默认文件
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
  char buffer[1024]; // 增大缓冲区，提高性能
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

// 注册 URI 处理器
esp_err_t register_static_file_server(httpd_handle_t server)
{
  int file_count = file_array.file_count; // 获取文件路径数量

  if (file_count > MAX_FILES)
  {
    ESP_LOGE(TAG, "Too many files to register, maximum is %d", MAX_FILES);
    return ESP_FAIL;
  }

  for (int i = 0; i < file_count; i++)
  {
    httpd_uri_t handler = {
        .uri = file_array.file_paths[i], // 直接使用 file_paths 中的路径
        .method = HTTP_GET,
        .handler = file_server_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Registering URI: %s", handler.uri);

    esp_err_t res = httpd_register_uri_handler(server, &handler);

    if (res != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to register URI handler for %s: %s", handler.uri, esp_err_to_name(res));
      return res;
    }
  }

  return ESP_OK;
}

// HTTP 服务器初始化
void start_http_server()
{

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 128;
  config.stack_size = 8192;
  // 启动 HTTP 服务器
  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK)
  {
    ESP_LOGI(TAG, "HTTP server started");
    esp_err_t ret = register_static_file_server(server); // 注册静态资源处理器
    if (ret == ESP_OK)
    {
      ESP_LOGI(TAG, "Static file server registered successfully");
      isInited = true;
    }
    else
    {
      ESP_LOGE(TAG, "Failed to register static file server: %s", esp_err_to_name(ret));
      httpd_stop(server);
    }
  }
  else
  {
    ESP_LOGE(TAG, "Failed to start HTTP server");
  }
}

bool httpd_is_inited()
{
  return isInited;
}
