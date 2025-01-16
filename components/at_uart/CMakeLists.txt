idf_component_register(SRCS "at_uart.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver at_config at_utils
                       )