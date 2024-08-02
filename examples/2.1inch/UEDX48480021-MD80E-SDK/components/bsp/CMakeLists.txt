file(GLOB_RECURSE SUB_BOARD ./sub_board/*.c)

idf_component_register(
    SRCS "bsp.c" ${SUB_BOARD}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver
    PRIV_REQUIRES esp_timer esp_lcd
)
