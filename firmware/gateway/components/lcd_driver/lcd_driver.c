#include "lcd_driver.h"

#include "esp_log.h"

static const char *TAG = "lcd_driver";

esp_err_t lcd_driver_init(void)
{
    ESP_LOGI(TAG,
             "lcd_driver contract placeholder (8-pin ST7789V2 contacts "
             "1..8 in header; backlight GPIO%d candidate pending physical "
             "verification; no esp_lcd_* call in this contract)",
             LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE);
    return ESP_OK;
}