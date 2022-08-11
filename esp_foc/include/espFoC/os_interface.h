#pragma once

typedef void (*foc_loop_runner) (void *arg);
typedef void*  esp_foc_event_handle_t;

int esp_foc_create_runner(foc_loop_runner runner, void *argument, int priority);
void esp_foc_sleep_ms(int sleep_ms);
void esp_foc_runner_yield(void);
float esp_foc_now_seconds(void);
void esp_foc_fpu_isr_enter(void);
void esp_foc_fpu_isr_leave(void);
void esp_foc_critical_enter(void);
void esp_foc_critical_leave(void);
esp_foc_event_handle_t esp_foc_get_event_handle(void);
void esp_foc_wait_notifier(void);
void esp_foc_send_notification(esp_foc_event_handle_t handle);
