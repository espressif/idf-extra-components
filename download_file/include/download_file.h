/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) Co. Ltd.
 */

#include <stdio.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for download_file
 */
typedef struct {
    size_t buffer_size;     /*!< Size of buffer to use for download */
    int timeout_ms;         /*!< Timeout for downloading */
    size_t download_task_stack; /*!< Stack size for download task */
    int download_task_priority; /*!< Priority for download task */
    bool skip_file_buffer; /*!< Skip FILE* stream buffer and write directly to the file descriptor */
    void *user_data;        /*!< User data to pass to callbacks */
    esp_err_t (*http_client_config_cb)(void *user_data, esp_http_client_config_t *http_client_config);    /*!< Callback to call to configure http client */
    esp_err_t (*http_client_post_init_cb)(void *user_data, esp_http_client_handle_t http_client); /*!< Callback to call after http client is initialized */
    void (*progress_cb)(void *user_data, size_t bytes_done, size_t bytes_total); /*!< Callback to call on progress */
} download_file_config_t;

#define DOWNLOAD_FILE_CONFIG_DEFAULT() { \
    .buffer_size = 1024, \
    .timeout_ms = 10000, \
    .download_task_stack = 4096, \
    .download_task_priority = 5, \
    .user_data = NULL, \
    .http_client_config_cb = NULL, \
    .http_client_post_init_cb = NULL, \
    .progress_cb = NULL, \
}

esp_err_t download_file(const char *url, FILE *f_out, const download_file_config_t *config);


#ifdef __cplusplus
}
#endif

