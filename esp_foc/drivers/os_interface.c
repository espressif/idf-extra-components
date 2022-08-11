#include <sys/cdefs.h>
#include <unistd.h>
#include "espFoC/esp_foc.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "xtensa/hal.h"

static const float scale = 0.000001f;
static uint32_t cp0_regs[18];
static uint32_t cp_state;
static portMUX_TYPE spinlock =  portMUX_INITIALIZER_UNLOCKED;

int esp_foc_create_runner(foc_loop_runner runner, void *argument, int priority)
{
    int ret = xTaskCreatePinnedToCore(runner,"", CONFIG_FOC_TASK_STACK_SIZE, argument, priority, NULL, APP_CPU_NUM);

    if (ret != pdPASS) {
        return -ESP_ERR_NO_MEM;
    }

    return 0;
}

void esp_foc_sleep_ms(int sleep_ms)
{
    usleep(sleep_ms * 1000);
}

void esp_foc_runner_yield(void)
{
    taskYIELD();
}

float esp_foc_now_seconds(void)
{
    float now = (float)esp_timer_get_time();
    return(now * scale);
}

/* the fpu in isr tricks below are required for xtensa based archs: */
void esp_foc_fpu_isr_enter(void)
{
    cp_state = xthal_get_cpenable();

    if(cp_state) {
        xthal_save_cp0(cp0_regs);   
    } else {
        xthal_set_cpenable(1);
    }
}

void esp_foc_fpu_isr_leave(void)
{
    if(cp_state) {
        xthal_restore_cp0(cp0_regs);
    } else {
        xthal_set_cpenable(0);
    }

}

void esp_foc_critical_enter(void)
{
    portENTER_CRITICAL(&spinlock);
}

void esp_foc_critical_leave(void)
{
    portEXIT_CRITICAL(&spinlock);
}

esp_foc_event_handle_t esp_foc_get_event_handle(void)
{
    return ((esp_foc_event_handle_t) xTaskGetCurrentTaskHandle());
}

void esp_foc_wait_notifier(void)
{
    ulTaskNotifyTake(pdFALSE ,portMAX_DELAY);
}

void esp_foc_send_notification(esp_foc_event_handle_t handle)
{
    BaseType_t wake;
    vTaskNotifyGiveFromISR((TaskHandle_t)handle, &wake);
    if (wake == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
