#include <sys/cdefs.h>
#include "espFoC/inverter_3pwm_ledc.h"
#include "hal/ledc_hal.h"
#include "esp_attr.h"

#define LEDC_FREQUENCY_HZ       20000
#define LEDC_RESOLUTION_STEPS   255.0

typedef struct {
    ledc_dev_t *hw;
    float voltage_to_duty_ratio;
    float dc_link_voltage;
    ledc_channel_t ledc_channel[3];
    esp_foc_inverter_callback_t notifier;
    void *arg;
    esp_foc_inverter_t interface;
}esp_foc_ledc_inverter;

static const ledc_timer_t ledc_timers[4] = {LEDC_TIMER_0,LEDC_TIMER_1,LEDC_TIMER_2,LEDC_TIMER_3};

static bool ledc_driver_configured = false;

DRAM_ATTR static esp_foc_ledc_inverter ledc[CONFIG_NOOF_AXIS];

IRAM_ATTR static void ledc_isr(void *arg) 
{
    esp_foc_fpu_isr_enter();

    esp_foc_ledc_inverter *obj = (esp_foc_ledc_inverter *)arg;

    obj->hw->int_clr.val = (LEDC_LSTIMER0_OVF_INT_ENA |
                        LEDC_LSTIMER1_OVF_INT_ENA |
                        LEDC_LSTIMER2_OVF_INT_ENA |
                        LEDC_LSTIMER3_OVF_INT_ENA );

    if(obj->notifier) {
        obj->notifier(obj->arg);
    }

    esp_foc_fpu_isr_leave();
}

/* This function is required because the ledc driver does not support update from 
 * ISR
 */
IRAM_ATTR static void ledc_update(esp_foc_ledc_inverter *obj, ledc_channel_t channel, float duty)
{
    /* set duty parameters */
    ledc_ll_set_hpoint(obj->hw, LEDC_LOW_SPEED_MODE, channel, 0);
    ledc_ll_set_duty_int_part(obj->hw, LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_ll_set_duty_direction(obj->hw, LEDC_LOW_SPEED_MODE, channel, LEDC_DUTY_DIR_INCREASE);    
    ledc_ll_set_duty_num(obj->hw, LEDC_LOW_SPEED_MODE, channel, 1);
    ledc_ll_set_duty_cycle(obj->hw, LEDC_LOW_SPEED_MODE, channel, 1);    
    ledc_ll_set_duty_scale(obj->hw, LEDC_LOW_SPEED_MODE, channel, 0);
    ledc_ll_ls_channel_update(obj->hw, LEDC_LOW_SPEED_MODE, channel);
    
    /* trigger the duty update */
    ledc_ll_set_sig_out_en(obj->hw, LEDC_LOW_SPEED_MODE, channel, true);
    ledc_ll_set_duty_start(obj->hw, LEDC_LOW_SPEED_MODE, channel, true);
}


IRAM_ATTR static float get_dc_link_voltage (esp_foc_inverter_t *self)
{
    esp_foc_ledc_inverter *obj = 
        __containerof(self, esp_foc_ledc_inverter, interface);

    return obj->dc_link_voltage;
}

IRAM_ATTR static void set_voltages(esp_foc_inverter_t *self,
                    float v_u,
                    float v_v,
                    float v_w)
{
    esp_foc_ledc_inverter *obj = 
        __containerof(self, esp_foc_ledc_inverter, interface);

    if(v_u > obj->dc_link_voltage) {
        v_u = obj->dc_link_voltage;
    } else if (v_u < 0.0f) {
        v_u = 0.0f;
    }

    if(v_v > obj->dc_link_voltage) {
        v_v = obj->dc_link_voltage;
    } else if (v_v < 0.0f) {
        v_v = 0.0f;
    }

    if(v_w > obj->dc_link_voltage) {
        v_w = obj->dc_link_voltage;
    } else if (v_w < 0.0f) {
        v_w = 0.0f;
    }

    ledc_update(obj, obj->ledc_channel[0], obj->voltage_to_duty_ratio * v_u);
    ledc_update(obj, obj->ledc_channel[1], obj->voltage_to_duty_ratio * v_v);
    ledc_update(obj, obj->ledc_channel[2], obj->voltage_to_duty_ratio * v_w);
}

IRAM_ATTR static void set_inverter_callback(esp_foc_inverter_t *self,
                        esp_foc_inverter_callback_t callback,
                        void *argument)
{
    esp_foc_ledc_inverter *obj = 
    __containerof(self, esp_foc_ledc_inverter, interface);

    obj->notifier = callback;
    obj->arg = argument;

    ledc_isr_register(ledc_isr, obj, ESP_INTR_FLAG_IRAM, NULL);
    obj->hw->int_ena.val |= (LEDC_LSTIMER0_OVF_INT_ENA |
                        LEDC_LSTIMER1_OVF_INT_ENA |
                        LEDC_LSTIMER2_OVF_INT_ENA |
                        LEDC_LSTIMER3_OVF_INT_ENA );
}

IRAM_ATTR static void phase_remap(esp_foc_inverter_t *self)
{
    (void)self;
}

IRAM_ATTR static float get_inverter_pwm_rate (esp_foc_inverter_t *self)
{
    (void)self;
    return (float)LEDC_FREQUENCY_HZ;
}

static esp_err_t inverter_3pwm_ledc_init()
{
    for (int i = 0; i < CONFIG_NOOF_AXIS; i++) {
        ledc_timer_config_t ledc_timer = {
            .duty_resolution = LEDC_TIMER_8_BIT,
            .freq_hz = LEDC_FREQUENCY_HZ,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = ledc_timers[i],
            .clk_cfg = LEDC_AUTO_CLK,
        };

        ledc_timer_config(&ledc_timer);
    }

    return ESP_OK;
}

esp_foc_inverter_t *inverter_3pwm_ledc_new(ledc_channel_t ch_u,
                                        ledc_channel_t ch_v,
                                        ledc_channel_t ch_w,
                                        int gpio_u,
                                        int gpio_v,
                                        int gpio_w,
                                        float dc_link_voltage,
                                        int port)
{
    if(port > CONFIG_NOOF_AXIS - 1) {
        return NULL;
    }

    if(!ledc_driver_configured) {
        inverter_3pwm_ledc_init();
        ledc_driver_configured = true;
    }

    ledc[port].dc_link_voltage = dc_link_voltage;
    ledc[port].interface.get_dc_link_voltage = get_dc_link_voltage;
    ledc[port].interface.set_voltages = set_voltages;
    ledc[port].interface.set_inverter_callback = set_inverter_callback;
    ledc[port].interface.phase_remap = phase_remap;
    ledc[port].interface.get_inverter_pwm_rate = get_inverter_pwm_rate;
    ledc[port].ledc_channel[0] = ch_u;
    ledc[port].ledc_channel[1] = ch_v;
    ledc[port].ledc_channel[2] = ch_w;

    ledc_channel_config_t ledc_channel[3] =  {
        {
            .channel    = ch_u,
            .duty       = 0,
            .gpio_num   = gpio_u,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = ledc_timers[port]
        },

        {
            .channel    = ch_v,
            .duty       = 0,
            .gpio_num   = gpio_v,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = ledc_timers[port]
        },

        {
            .channel    = ch_w,
            .duty       = 0,
            .gpio_num   = gpio_w,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = ledc_timers[port]
        },
    };

    for (int ch = 0; ch < 3; ch++) {
        esp_err_t err = ledc_channel_config(&ledc_channel[ch]);
        if(err != ESP_OK) {
            return NULL;
        }
    }

    ledc[port].hw = LEDC_LL_GET_HW();
    ledc[port].voltage_to_duty_ratio = LEDC_RESOLUTION_STEPS / ledc[port].dc_link_voltage;

    return &ledc[port].interface;
}