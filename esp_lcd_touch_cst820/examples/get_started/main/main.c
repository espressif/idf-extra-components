#include "esp_lcd_touch_cst820.h"
#include "esp_log.h"

static const char *TAG = "cst820_example";

void app_main(void)
{
    const esp_lcd_panel_io_i2c_config_t io_config =
        ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();
    ESP_LOGI(TAG, "CST820 example; address=0x%02x, bus=%u Hz",
             io_config.dev_addr, (unsigned)io_config.scl_speed_hz);
    ESP_LOGI(TAG, "Create an i2c_master bus and panel IO, then pass x_max/y_max as inclusive coordinate maxima.");
}
