idf_component_register(
  SRCS "wifi_sta_driver.c" 
  INCLUDE_DIRS "."
  REQUIRES esp_wifi esp_event nvs_flash 
)
