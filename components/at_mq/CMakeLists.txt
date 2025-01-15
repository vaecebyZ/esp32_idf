idf_component_register(SRCS "at_mq.c"
                       INCLUDE_DIRS "."
                       REQUIRES at_check at_uart at_http at_config at_utils
                       PRIV_REQUIRES json
                       )