#include "rx8130ce.h"
#include "esp_log.h"

static const char *TAG = "rx8130ce_example";

void app_main(void)
{
    const rx8130ce_config_t config = RX8130CE_CONFIG_DEFAULT();
    ESP_LOGI(TAG, "RX8130CE example; address=0x%02x, bus=%u Hz",
             config.device_address, (unsigned)config.scl_speed_hz);
    ESP_LOGI(TAG, "Use RX8130CE_CONFIG_DEFAULT(), create on an initialized i2c_master bus, then set and read the calendar.");
}
