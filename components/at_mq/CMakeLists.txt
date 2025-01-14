idf_component_register(SRCS "at_mq.c"
                       INCLUDE_DIRS "."
                       REQUIRES at_check at_uart at_http
                       PRIV_REQUIRES json
                       )