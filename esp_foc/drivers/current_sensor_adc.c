#include <sys/cdefs.h>
#include <stdbool.h>
#include "espFoC/current_sensor_adc.h"
#include "hal/adc_hal.h"
#include "esp_attr.h"
#include "esp_log.h"

#if CONFIG_IDF_TARGET_ESP32S3

#define ADC_RESULT_BYTE     4
#define ADC_CONV_LIMIT_EN   0
#define ADC_CONV_MODE       ADC_CONV_BOTH_UNIT
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2

#define ISENSOR_ADC_BUFFER_SIZE      1024
#define GET_UNIT(x)        ((x>>3) & 0x1)

static const char *TAG = "ESP_FOC_ISENSOR";

typedef struct {
    float adc_to_current_scale;
    isensor_values_t currents;
    esp_foc_isensor_t interface;
    uint8_t noof_channels;
}isensor_adc_t;

DRAM_ATTR static isensor_adc_t isensor_adc;
static bool adc_initialized = false;

static void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask, adc_channel_t *channel, uint8_t channel_num)
{
    adc_digi_init_config_t adc_dma_config = {
        .max_store_buf_size = ISENSOR_ADC_BUFFER_SIZE,
        .conv_num_each_intr = channel_num,
        .adc1_chan_mask = adc1_chan_mask,
        .adc2_chan_mask = adc2_chan_mask,
    };
   adc_digi_initialize(&adc_dma_config);

    adc_digi_configuration_t dig_cfg = {
        .conv_limit_en = ADC_CONV_LIMIT_EN,
        .conv_limit_num = 250,
        .sample_freq_hz = SOC_ADC_SAMPLE_FREQ_THRES_HIGH,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        uint8_t unit = GET_UNIT(channel[i]);
        uint8_t ch = channel[i] & 0x7;
        adc_pattern[i].atten = ADC_ATTEN_DB_0;
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    adc_digi_controller_configure(&dig_cfg);
}


IRAM_ATTR static void fetch_isensors(esp_foc_isensor_t *self, isensor_values_t *values)
{
    isensor_adc_t *obj = 
        __containerof(self, isensor_adc_t, interface);

    uint32_t ret_num;
    adc_digi_output_data_t result[obj->noof_channels];
 
    esp_err_t err = adc_digi_read_bytes((uint8_t *)&result, 
                                    obj->noof_channels * sizeof(adc_digi_output_data_t),
                                    &ret_num,
                                    0);

    if(err == ESP_OK) {

        obj->currents.iu_axis_0 = (float)result[0].type2.data * obj->adc_to_current_scale;
        obj->currents.iv_axis_0 = (float)result[1].type2.data * obj->adc_to_current_scale;
        obj->currents.iw_axis_0 = obj->currents.iu_axis_0 + obj->currents.iv_axis_0;

        if(obj->noof_channels > 2) {
            obj->currents.iu_axis_1 = (float)result[2].type2.data * obj->adc_to_current_scale;
            obj->currents.iv_axis_1 = (float)result[3].type2.data * obj->adc_to_current_scale;
            obj->currents.iw_axis_1 = obj->currents.iu_axis_1 + obj->currents.iv_axis_1;
        } 
    }
}

IRAM_ATTR static void sample_isensors(esp_foc_isensor_t *self)
{
    (void)self;    
    adc_digi_start();
}


esp_foc_isensor_t *isensor_adc_new(esp_foc_isensor_adc_config_t *config,
                                float adc_to_current_scale)

{
    uint16_t adc1_chan_mask = 0;
    uint16_t adc2_chan_mask = 0;
    uint8_t noof_channels = config->noof_axis * 2;

    if(adc_initialized == true) {
        return &isensor_adc.interface;
    }

    adc_initialized = true;

    adc_digi_stop();

    for(uint8_t i = 0; i < noof_channels; i++) {
        if(config->axis_channels[i] != 0xFF) {
            adc1_chan_mask |= (1 << config->axis_channels[i]);
        }
    } 

    continuous_adc_init(adc1_chan_mask, adc2_chan_mask, config->axis_channels, noof_channels);

    isensor_adc.adc_to_current_scale = adc_to_current_scale;
    isensor_adc.interface.fetch_isensors = fetch_isensors;
    isensor_adc.interface.sample_isensors = sample_isensors;
    isensor_adc.noof_channels = noof_channels;

    return &isensor_adc.interface;
}

#endif