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

/**
 * @brief Handle to a REPL instance.
 */
typedef struct esp_repl_instance *esp_repl_handle_t;

/**
 * @brief Function prototype for reading input for the REPL.
 *
 * @param ctx User-defined context pointer.
 * @param buf Buffer to store the read data.
 * @param buf_size Size of the buffer in bytes.
 *
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*esp_repl_reader_fn)(void *ctx, char *buf, size_t buf_size);

/**
 * @brief Reader configuration structure for the REPL.
 */
typedef struct esp_repl_reader {
    esp_repl_reader_fn func; /**!< Function to read input */
    void *ctx;               /**!< Context passed to the reader function */
} esp_repl_reader_t;

/**
 * @brief Function prototype called before executing a command.
 *
 * @param ctx User-defined context pointer.
 * @param buf Buffer containing the command.
 * @param reader_ret_val Return value from the reader function.
 *
 * @return ESP_OK to continue execution, error code to abort.
 */
typedef esp_err_t (*esp_repl_pre_executor_fn)(void *ctx, char *buf, const esp_err_t reader_ret_val);

/**
 * @brief Pre-executor configuration structure for the REPL.
 */
typedef struct esp_repl_pre_executor {
    esp_repl_pre_executor_fn func; /**!< Function to run before command execution */
    void *ctx;                      /**!< Context passed to the pre-executor function */
} esp_repl_pre_executor_t;

/**
 * @brief Function prototype to execute a REPL command.
 *
 * @param ctx User-defined context pointer.
 * @param buf Null-terminated command string.
 * @param ret_val Pointer to store the command return value.
 *
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*esp_repl_executor_fn)(void *ctx, const char *buf, int *ret_val);

/**
 * @brief Executor configuration structure for the REPL.
 */
typedef struct esp_repl_executor {
    esp_repl_executor_fn func; /**!< Function to execute commands */
    void *ctx;                  /**!< Context passed to the executor function */
} esp_repl_executor_t;

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
typedef esp_err_t (*esp_repl_post_executor_fn)(void *ctx, const char *buf, const esp_err_t executor_ret_val, const int cmd_ret_val);

/**
 * @brief Post-executor configuration structure for the REPL.
 */
typedef struct esp_repl_post_executor {
    esp_repl_post_executor_fn func; /**!< Function called after command execution */
    void *ctx;                       /**!< Context passed to the post-executor function */
} esp_repl_post_executor_t;

/**
 * @brief Function prototype called when the REPL is stopping.
 *
 * This callback allows the user to unblock the reader (or perform other
 * cleanup) so that the REPL can return from `esp_repl()`.
 *
 * @param ctx User-defined context pointer.
 * @param handle Handle to the REPL instance.
 */
typedef void (*esp_repl_on_stop_fn)(void *ctx, esp_repl_handle_t handle);

/**
 * @brief Stop callback configuration structure for the REPL.
 */
typedef struct esp_repl_on_stop {
    esp_repl_on_stop_fn func; /**!< Function called when REPL stop is requested */
    void *ctx;                /**!< Context passed to the on_stop function */
} esp_repl_on_stop_t;

/**
 * @brief Function prototype called when the REPL exits.
 *
 * @param ctx User-defined context pointer.
 * @param handle Handle to the REPL instance.
 */
typedef void (*esp_repl_on_exit_fn)(void *ctx, esp_repl_handle_t handle);

/**
 * @brief Exit callback configuration structure for the REPL.
 */
typedef struct esp_repl_on_exit {
    esp_repl_on_exit_fn func; /**!< Function called on REPL exit */
    void *ctx;                /**!< Context passed to the exit function */
} esp_repl_on_exit_t;

/**
 * @brief Configuration structure to initialize a REPL instance.
 */
typedef struct esp_repl_config {
    size_t max_cmd_line_size;        /**!< Maximum allowed command line size */
    esp_repl_reader_t reader;        /**!< Reader callback and context */
    esp_repl_pre_executor_t pre_executor;   /**!< Pre-executor callback and context */
    esp_repl_executor_t executor;           /**!< Executor callback and context */
    esp_repl_post_executor_t post_executor; /**!< Post-executor callback and context */
    esp_repl_on_stop_t on_stop;             /**!< Stop callback and context */
    esp_repl_on_exit_t on_exit;             /**!< Exit callback and context */
} esp_repl_config_t;

/**
 * @brief Create a REPL instance.
 *
 * @param handle Pointer to store the created REPL instance handle.
 * @param config Pointer to the configuration structure.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_repl_create(esp_repl_handle_t *handle, const esp_repl_config_t *config);

/**
 * @brief Destroy a REPL instance.
 *
 * @param handle REPL instance handle to destroy.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_repl_destroy(esp_repl_handle_t handle);

/**
 * @brief Start a REPL instance.
 *
 * @param handle REPL instance handle.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_repl_start(esp_repl_handle_t handle);

/**
 * @brief Stop a REPL instance.
 *
 * @param handle REPL instance handle.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t esp_repl_stop(esp_repl_handle_t handle);

/**
 * @brief Run the REPL loop.
 *
 * @param handle REPL instance handle.
 */
void esp_repl(esp_repl_handle_t handle);

#ifdef __cplusplus
}
#endif
