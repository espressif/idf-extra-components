
/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_repl.h"
#include "esp_err.h"
#include "esp_commands.h"
#include "esp_linenoise.h"
typedef enum {
    ESP_REPL_STATE_RUNNING,
    ESP_REPL_STATE_STOPPED
} esp_repl_state_e;

typedef struct esp_repl_state {
    esp_repl_state_e state;
    TaskHandle_t task_hdl;
    SemaphoreHandle_t mux;
} esp_repl_state_t;

typedef struct esp_repl_instance {
    esp_repl_config_t config;
    esp_repl_state_t state;
} esp_repl_instance_t;

#define ESP_REPL_CHECK_INSTANCE(handle) do {    \
        if(handle == NULL) {                    \
            return ESP_ERR_INVALID_ARG;         \
        }                                       \
    } while(0)

esp_err_t esp_repl_create(const esp_repl_config_t *config, esp_repl_handle_t *out_handle)
{
    if (!config || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->linenoise_handle == NULL) ||
            (config->max_cmd_line_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_repl_instance_t *instance = malloc(sizeof(esp_repl_instance_t));
    if (!instance) {
        return ESP_ERR_NO_MEM;
    }

    instance->config = *config;
    instance->state.state = ESP_REPL_STATE_STOPPED;
    instance->state.mux = xSemaphoreCreateMutex();
    if (!instance->state.mux) {
        free(instance);
        return ESP_FAIL;
    }

    /* take the mutex right away to prevent the task to start running until
    * the user explicitly calls esp_repl_start */
    xSemaphoreTake(instance->state.mux, portMAX_DELAY);

    *out_handle = instance;
    return ESP_OK;
}

esp_err_t esp_repl_destroy(esp_repl_handle_t handle)
{
    ESP_REPL_CHECK_INSTANCE(handle);
    esp_repl_state_t *state = &handle->state;

    /* the instance has to be not running for esp_repl to destroy it */
    if (state->state != ESP_REPL_STATE_STOPPED) {
        return ESP_ERR_INVALID_STATE;
    }

    vSemaphoreDelete(state->mux);

    free(handle);

    return ESP_OK;
}

esp_err_t esp_repl_start(esp_repl_handle_t handle)
{
    ESP_REPL_CHECK_INSTANCE(handle);
    esp_repl_state_t *state = &handle->state;

    if (state->state != ESP_REPL_STATE_STOPPED) {
        return ESP_ERR_INVALID_STATE;
    }
    state->state = ESP_REPL_STATE_RUNNING;
    xSemaphoreGive(state->mux);

    return ESP_OK;
}

esp_err_t esp_repl_stop(esp_repl_handle_t handle)
{
    ESP_REPL_CHECK_INSTANCE(handle);
    esp_repl_config_t *config = &handle->config;
    esp_repl_state_t *state = &handle->state;

    if (state->state != ESP_REPL_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }

    /* update the state to force the while loop in esp_repl to return */
    state->state = ESP_REPL_STATE_STOPPED;

    /** This function forces esp_linenoise_get_line() to return.
     *
     * Return Values:
     *   - ESP_OK: Returned if the user has not provided a custom read and the abort operation succeeds.
     *   - ESP_ERR_INVALID_STATE: Returned if the user has provided a custom read. In this case, the user
     *     is responsible for implementing an abort mechanism that ensures a successful return from
     *     their custom read. This can be achieved by placing the logic in the on_stop callback.
     *
     * Behavior:
     *   - When a custom read is registered, ESP_ERR_INVALID_STATE indicates that esp_repl_stop() cannot
     *     forcibly return from the read. The user must handle the return of their custom read via on_stop().
     *   - From the perspective of esp_repl_stop(), this scenario is treated as successful, and its
     *     return value should be set to ESP_OK.
     */
    esp_err_t ret_val = esp_linenoise_abort(config->linenoise_handle);
    if (ret_val == ESP_ERR_INVALID_STATE) {
        ret_val = ESP_OK;
    }

    /* Call the on_stop callback to let the user unblock esp_linenoise
    * if a custom read is provided */
    if (config->on_stop.func != NULL) {
        config->on_stop.func(config->on_stop.ctx, handle);
    }

    /* Wait for esp_repl() to finish and signal completion, in the event of
     * esp_repl_stop() is called from the same task running esp_repl() (e.g.,
     * called from a "quit" command), do not take the mutex to avoid a deadlock.
     *
     * If esp_repl_stop() is called from the same task, it assures that this task
     * is not blocking in esp_linenoise_get_line() so the while loop in esp_repl()
     * will return as we updated the state above */
    if (state->task_hdl && state->task_hdl != xTaskGetCurrentTaskHandle()) {
        xSemaphoreTake(state->mux, portMAX_DELAY);
    }

    return ret_val;
}

void esp_repl(esp_repl_handle_t handle)
{
    if (!handle) {
        return;
    }

    esp_repl_config_t *config = &handle->config;
    esp_repl_state_t *state = &handle->state;

    /* get the task handle of the task running this function.
     * It is necessary to gather this information in case esp_repl_stop()
     * is called from the same task as the one running esp_repl() (e.g.,
     * through the execution of a command) */
    state->task_hdl = xTaskGetCurrentTaskHandle();

    /* allocate memory for the command line buffer */
    const size_t cmd_line_size = config->max_cmd_line_size;
    char *cmd_line = calloc(1, cmd_line_size);
    if (!cmd_line) {
        return;
    }

    /* Waiting for task notify. This happens when `esp_repl_start`
    * function is called. */
    xSemaphoreTake(state->mux, portMAX_DELAY);

    esp_linenoise_handle_t l_hdl = config->linenoise_handle;
    esp_command_set_handle_t c_set = config->command_set_handle;

    /* REPL loop */
    while (state->state == ESP_REPL_STATE_RUNNING) {

        /* try to read a command line */
        const esp_err_t read_ret = esp_linenoise_get_line(l_hdl, cmd_line, cmd_line_size);

        /* Add the command to the history */
        esp_linenoise_history_add(l_hdl, cmd_line);

        /* Save command history to filesystem */
        if (config->history_save_path) {
            esp_linenoise_history_save(l_hdl, config->history_save_path);
        }

        /* forward the raw command line to the pre executor callback (e.g., save in history).
        * this callback is not necessary for the user to register, continue if it isn't */
        if (config->pre_executor.func != NULL) {
            config->pre_executor.func(config->pre_executor.ctx, cmd_line, read_ret);
        }

        /* at this point, if the command is NULL, skip the executing part */
        if (read_ret != ESP_OK) {
            continue;
        }

        /* try to run the command */
        int cmd_func_ret;
        const esp_err_t exec_ret = esp_commands_execute(c_set, -1, cmd_line, &cmd_func_ret);

        /* forward the raw command line to the post executor callback (e.g., save in history).
        * this callback is not necessary for the user to register, continue if it isn't */
        if (config->post_executor.func != NULL) {
            config->post_executor.func(config->post_executor.ctx, cmd_line, exec_ret, cmd_func_ret);
        }

        /* reset the cmd_line for next loop */
        memset(cmd_line, 0x00, cmd_line_size);
    }

    /* free the memory allocated for the cmd_line buffer */
    free(cmd_line);

    /* release the semaphore to indicate esp_repl_stop that the esp_repl returned */
    xSemaphoreGive(state->mux);

    /* call the on_exit callback before returning from esp_repl */
    if (config->on_exit.func != NULL) {
        config->on_exit.func(config->on_exit.ctx, handle);
    }
}
