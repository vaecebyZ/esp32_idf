idf_component_register(SRCS "sdcard.c"
                       INCLUDE_DIRS "."
                       REQUIRES  fatfs esp_http_server
                       )
