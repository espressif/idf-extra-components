/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Delta OTA HTTPS example's test file
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include "esp_https_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "test_local_server_ota.h"
#include "protocol_examples_common.h"
#include "esp_partition.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define OTA_URL_SIZE 256
#define PARTITION_READ_BUFFER_SIZE 256
#define PARTITION_READ_SIZE PARTITION_READ_BUFFER_SIZE

static const char *TAG = "test_local_server_ota";
static size_t patch_size = 0;

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPG_URL_FROM_STDIN
void delta_ota_test_firmware_data_from_stdin(const char **data)
{
    char input_buf[OTA_URL_SIZE];
    if (strcmp(*data, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fflush(stdin);
        char *url = NULL;
        char *tokens[OTA_URL_SIZE];
        char *saveptr;
        int token_count = 0;

        if (fgets(input_buf, OTA_URL_SIZE, stdin) == NULL) {
            ESP_LOGE(TAG, "Failed to read URL from stdin");
            abort();
        }
        int len = strlen(input_buf);
        if (len == 0) {
            ESP_LOGE(TAG, "Empty URL read from stdin");
            abort();
        }
        if (input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
            len--;
        }
        char *token = strtok_r(input_buf, " ", &saveptr);
        if (token == NULL) {
            ESP_LOGE(TAG, "No URL token found in input");
            return;
        }
        // First token is the URL
        url = token;
        tokens[token_count++] = url;
        // Process remaining tokens
        while ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
            tokens[token_count++] = token;
        }
        // Require patch_size to be provided (at least 2 tokens: URL and patch_size)
        if (token_count < 2) {
            ESP_LOGE(TAG, "Expected URL and patch_size, but only got %d token(s)", token_count);
            return;
        }
        *data = strdup(tokens[0]);
        // Assign the URL and additional data after the loop
        if (token_count > 1) {
            ESP_LOGI(TAG, "patch_size: %s\n", tokens[1]);
            patch_size = atoi(tokens[1]); // Assuming the next token is the patch size
        }
        // Tokens are collected in the tokens array
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
}
#endif

/* An HTTP GET handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/octet-stream");

    // Find the patch_data partition where pytest writes the patch
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                               ESP_PARTITION_SUBTYPE_ANY, "patch_data");

    if (p == NULL) {
        ESP_LOGE(TAG, "patch_data partition not found");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Partition not found");
        return ESP_FAIL;
    }

    if (patch_size == 0) {
        ESP_LOGE(TAG, "Patch size is 0");
        return ESP_FAIL;
    }

    int image_len = patch_size;
    char buffer[PARTITION_READ_BUFFER_SIZE];
    int size = PARTITION_READ_SIZE;
    int offset = 0;

    do {
        /* Read file in chunks into the scratch buffer */
        if (offset + size > image_len) {
            size = image_len - offset;
        }
        if (size == 0) {
            break;
        }
        esp_err_t ret = esp_partition_read(p, offset, buffer, size);
        if (ret == ESP_OK) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, buffer, size) != ESP_OK) {
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "Partition read failed: %s", esp_err_to_name(ret));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read partition");
            return ESP_FAIL;
        }
        offset += size;

        /* Keep looping till the whole file is sent */
    } while (offset < image_len);

    ESP_LOGI(TAG, "Patch file sending complete");

    // Set headers
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t root_head_handler(httpd_req_t *req)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                       ESP_PARTITION_SUBTYPE_ANY, "patch_data");

    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition not found");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Partition not found");
        return ESP_FAIL;
    }

    if (patch_size == 0) {
        return ESP_FAIL;
    }

    // Get the size of the patch
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    httpd_resp_set_hdr(req, "Connection", "close");

    // Complete HEAD response with no body
    return httpd_resp_send(req, NULL, patch_size); // No body for HEAD method
}

static const httpd_uri_t get_root = {
    .uri       = "/patch.bin",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

static const httpd_uri_t head_root = {
    .uri       = "/patch.bin",
    .method    = HTTP_HEAD,
    .handler   = root_head_handler
};

esp_err_t delta_ota_test_start_webserver(void)
{
    httpd_handle_t server = NULL;
    // Start the httpd server
    ESP_LOGI(TAG, "Starting HTTPS server for CI test");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Error starting server!");
        return ret;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &get_root);
    httpd_register_uri_handler(server, &head_root);
    return ESP_OK;
}
