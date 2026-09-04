#include "fusb303b.h"
#include "esp_log.h"

static const char *TAG = "fusb303b_example";

void app_main(void)
{
    const fusb303b_config_t config = FUSB303B_CONFIG_DEFAULT();
    ESP_LOGI(TAG, "FUSB303B example; address=0x%02x, bus=%u Hz",
             config.device_address, (unsigned)config.scl_speed_hz);
    ESP_LOGI(TAG, "Create the driver on an initialized i2c_master bus, then set the role before enabling it.");
}
