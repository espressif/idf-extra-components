#include "tg28_sw.h"
#include "esp_log.h"

static const char *TAG = "tg28_sw_example";

void app_main(void)
{
    const tg28_sw_config_t config = TG28_SW_CONFIG_DEFAULT();
    ESP_LOGI(TAG, "TG28_SW example; address=0x%02x, bus=%u Hz",
             config.device_address, (unsigned)config.scl_speed_hz);
    ESP_LOGI(TAG, "Create on an initialized i2c_master bus; choose regulator and charger policy in the board layer.");
}
