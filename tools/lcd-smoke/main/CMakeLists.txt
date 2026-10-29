idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES lcd_smoke_pattern esp_lcd esp_common driver heap
)
