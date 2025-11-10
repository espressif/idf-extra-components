/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"
#include "esp_linenoise.h"
#include "esp_cli_commands.h"

/**
 * @brief Handle to a esp_cli instance.
 */
typedef struct esp_cli_instance *esp_cli_handle_t;

/**
 * @brief Function prototype called at the beginning of esp_cli().
 *
 * @param ctx User-defined context pointer.
 * @param handle Handle to the esp_cli instance.
 */
typedef void (*esp_cli_on_enter_fn)(void *ctx, esp_cli_handle_t handle);

/**
 * @brief Enter callback configuration structure for the esp_cli.
 */
typedef struct esp_cli_on_enter {
    esp_cli_on_enter_fn func; /**!< Function called at the beginning of esp_cli() */
    void *ctx;                /**!< Context passed to the enter function */
} esp_cli_on_enter_t;

/**
 * @brief Function prototype called before executing a command.
 *
 * @param ctx User-defined context pointer.
 * @param buf Buffer containing the command.
 * @param reader_ret_val Return value from the reader function.
 *
 * @return ESP_OK to continue execution, error code to abort.
 */
typedef esp_err_t (*esp_cli_pre_executor_fn)(void *ctx, const char *buf, esp_err_t reader_ret_val);

/**
 * @brief Pre-executor configuration structure for the esp_cli.
 */
typedef struct esp_cli_pre_executor {
    esp_cli_pre_executor_fn func; /**!< Function to run before command execution */
    void *ctx;                      /**!< Context passed to the pre-executor function */
} esp_cli_pre_executor_t;

/**
 * @brief Function prototype called after executing a command.
 *
 * @param ctx User-defined context pointer.
 * @param buf Command that was executed.
 * @param executor_ret_val Return value from the executor function.
 * @param cmd_ret_val Command-specific return value.
 *
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*esp_cli_post_executor_fn)(void *ctx, const char *buf, esp_err_t executor_ret_val, int cmd_ret_val);

/**
 * @brief Post-executor configuration structure for the esp_cli.
 */
typedef struct esp_cli_post_executor {
    esp_cli_post_executor_fn func; /**!< Function called after command execution */
    void *ctx;                       /**!< Context passed to the post-executor function */
} esp_cli_post_executor_t;

/**
 * @brief Function prototype called when the esp_cli is stopping.
 *
 * This callback allows the user to unblock the reader (or perform other
 * cleanup) so that the esp_cli can return from `esp_cli()`.
 *
 * @param ctx User-defined context pointer.
 * @param handle Handle to the esp_cli instance.
 */
typedef void (*esp_cli_on_stop_fn)(void *ctx, esp_cli_handle_t handle);

/**
 * @brief Stop callback configuration structure for the esp_cli.
 */
typedef struct esp_cli_on_stop {
    esp_cli_on_stop_fn func; /**!< Function called when esp_cli stop is requested */
    void *ctx;                /**!< Context passed to the on_stop function */
} esp_cli_on_stop_t;

/**
 * @brief Function prototype called when the esp_cli exits.
 *
 * @param ctx User-defined context pointer.
 * @param handle Handle to the esp_cli instance.
 */
typedef void (*esp_cli_on_exit_fn)(void *ctx, esp_cli_handle_t handle);

/**
 * @brief Exit callback configuration structure for the esp_cli.
 */
typedef struct esp_cli_on_exit {
    esp_cli_on_exit_fn func; /**!< Function called on esp_cli exit */
    void *ctx;                /**!< Context passed to the exit function */
} esp_cli_on_exit_t;

/**
 * @brief Configuration structure to initialize a esp_cli instance.
 */
typedef struct esp_cli_config {
    esp_linenoise_handle_t linenoise_handle;    /**!< Handle to the esp_linenoise instance */
    esp_cli_command_set_handle_t command_set_handle;   /**!< Handle to a set of commands */
    size_t max_cmd_line_size;                   /**!< Maximum allowed command line size */
    const char *history_save_path;              /**!< Path to file to save the history */
    esp_cli_on_enter_t on_enter;               /**!< Enter callback and context */
    esp_cli_pre_executor_t pre_executor;       /**!< Pre-executor callback and context */
    esp_cli_post_executor_t post_executor;     /**!< Post-executor callback and context */
    esp_cli_on_stop_t on_stop;                 /**!< Stop callback and context */
    esp_cli_on_exit_t on_exit;                 /**!< Exit callback and context */
} esp_cli_config_t;

/**
 * @brief Create a esp_cli instance.
 *
 * @param config Pointer to the configuration structure.
 * @param out_handle Pointer to store the created esp_cli instance handle.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_cli_create(const esp_cli_config_t *config, esp_cli_handle_t *out_handle);

/**
 * @brief Destroy a esp_cli instance.
 *
 * @param handle esp_cli instance handle to destroy.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_cli_destroy(esp_cli_handle_t handle);

/**
 * @brief Start a esp_cli instance.
 *
 * @param handle esp_cli instance handle.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_cli_start(esp_cli_handle_t handle);

/**
 * @brief Stop a esp_cli instance.
 *
 * @note This function will internally call 'esp_linenoise_abort' first to try to return from
 * 'esp_linenoise_get_line'. If the user has provided a custom read to the esp_linenoise
 * instance used by the esp_cli instance, it is the responsibility of the user to provide
 * the mechanism to return from this custom read by providing a callback to the 'on_stop' field
 * in the esp_cli_config_t.
 *
 * Return Values:
 *   - ESP_OK: Returned if the user has not provided a custom read and the abort operation succeeds.
 *   - ESP_ERR_INVALID_STATE: Returned if the user has provided a custom read. In this case, the user
 *     is responsible for implementing an abort mechanism that ensures a successful return from
 *     their custom read. This can be achieved by placing the logic in the on_stop callback.
 *
 * Behavior:
 *   - When a custom read is registered, ESP_ERR_INVALID_STATE indicates that esp_cli_stop() cannot
 *     forcibly return from the read. The user must handle the return themselves via on_stop().
 *   - From the perspective of esp_cli_stop(), this scenario is treated as successful, and its
 *     return value should be set to ESP_OK.
 *
 * @param handle esp_cli instance handle.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_cli_stop(esp_cli_handle_t handle);

/**
 * @brief Run the esp_cli loop.
 *
 * @param handle esp_cli instance handle.
 */
void esp_cli(esp_cli_handle_t handle);

#ifdef __cplusplus
}
#endif
