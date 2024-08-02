idf_component_register(
    SRC_DIRS
        "."
    INCLUDE_DIRS
        "."
    REQUIRES
        "esp_lcd"
        "driver"
)
add_prebuilt_library(bsp "${CMAKE_CURRENT_SOURCE_DIR}/libbsp.a"
                        REQUIRES driver esp_lcd lvgl__lvgl)
target_link_libraries(${COMPONENT_TARGET} INTERFACE bsp)