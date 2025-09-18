
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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
    SemaphoreHandle_t mux;
} esp_repl_state_t;

typedef struct esp_repl_instance {
    esp_repl_instance_handle_t self;
    esp_repl_config_t config;
    esp_repl_state_t state;
} esp_repl_instance_t;

#define ESP_REPL_CHECK_INSTANCE(handle) do {                                                          \
    if((handle == NULL) || ((esp_repl_instance_t*)handle->self != (esp_repl_instance_t*)handle)) {    \
        return ESP_ERR_INVALID_ARG;                                                                   \
    }                                                                                                 \
} while(0)

esp_err_t   esp_repl_create(esp_repl_instance_handle_t *handle, const esp_repl_config_t *config)
{
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
    instance->self = instance;
    instance->state.mux = xSemaphoreCreateMutex();
    if (!instance->state.mux) {
        free(instance);
        return ESP_FAIL;
    }

    /* take the mutex right away to prevent the task to start running until
     * the user explicitly calls esp_repl_start */
    xSemaphoreTake(instance->state.mux, portMAX_DELAY);

    *handle = instance;
    return ESP_OK;
}

esp_err_t esp_repl_destroy(esp_repl_instance_handle_t handle)
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

esp_err_t esp_repl_start(esp_repl_instance_handle_t handle)
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

esp_err_t esp_repl_stop(esp_repl_instance_handle_t handle)
{
    ESP_REPL_CHECK_INSTANCE(handle);
    esp_repl_config_t *config = &handle->config;
    esp_repl_state_t *state = &handle->state;

    if (state->state != ESP_REPL_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }

    /* update the state to force the while loop in esp_repl to return */
    state->state = ESP_REPL_STATE_STOPPED;

    /* Call the on_stop callback to let the user unblock reader.func, if provided */
    if (config->on_stop.func != NULL) {
        config->on_stop.func(config->on_stop.ctx, handle);
    }

    /* Wait for esp_repl() to finish and signal completion */
    xSemaphoreTake(state->mux, portMAX_DELAY);

    /* give it back so destroy can also take/give symmetrically */
    xSemaphoreGive(state->mux);

    return ESP_OK;
}

void esp_repl(esp_repl_instance_handle_t handle)
{
    if (!handle || handle->self != handle) {
        return;
    }

    esp_repl_config_t *config = &handle->config;
    esp_repl_state_t *state = &handle->state;

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
        printf("returned from get line, %s\n", cmd_line);
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
        const esp_err_t exec_ret = esp_commands_execute(c_set, cmd_line, &cmd_func_ret);

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
