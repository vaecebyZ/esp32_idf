idf_component_register(SRCS "at_http.c"
                       INCLUDE_DIRS "."
                       REQUIRES at_check at_uart at_config
                       PRIV_REQUIRES json
                       )