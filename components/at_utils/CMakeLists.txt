idf_component_register(SRCS "at_utils.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer at_uart
                       PRIV_REQUIRES esp_netif at_check at_config
                       )