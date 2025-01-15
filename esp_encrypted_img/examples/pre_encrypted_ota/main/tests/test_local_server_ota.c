/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Pre Encrypted HTTPS OTA example's test file

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_https_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "test_local_server_ota.h"
#include "protocol_examples_common.h"

#define OTA_URL_SIZE 256
#define PARTITION_READ_BUFFER_SIZE 256
#define PARTITION_READ_SIZE PARTITION_READ_BUFFER_SIZE

static const char *TAG = "test_local_server_ota";
static size_t binary_size;

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
void example_test_firmware_data_from_stdin(const char **data)
{
    char input_buf[OTA_URL_SIZE];
    if (strcmp(*data, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fflush(stdin);
        char *url = NULL;
        char *tokens[OTA_URL_SIZE], *saveptr;
        int token_count = 0;

        fgets(input_buf, OTA_URL_SIZE, stdin);
        int len = strlen(input_buf);
        if (len > 0 && input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
        }
        char *token = strtok_r(input_buf, " ", &saveptr);
        if (token == NULL) {
            return;
        }
        // First token is the URL
        url = token;
        tokens[token_count++] = url;
        // Process remaining tokens
        while ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
            tokens[token_count++] = token;
        }
        // Return if no further data is captured
        if (strchr(input_buf, ' ') != NULL) {
            return;
        }
        *data = strdup(tokens[0]);
        // Assign the URL and additional data after the loop
        if (token_count > 1) {
            ESP_LOGI(TAG, "binary_size: %s\n", tokens[1]);
            binary_size = atoi(tokens[1]); // Assuming the next token is the binary size
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
    httpd_resp_set_type(req, "text/plain");
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                               ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);


    assert(p != NULL);

    if (binary_size == 0) {
        return ESP_FAIL;
    }

    int image_len = binary_size;
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
        }
        offset += size;

        /* Keep looping till the whole file is sent */
    } while (offset <= image_len);

    ESP_LOGI(TAG, "File sending complete");

    // Set headers
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t root_head_handler(httpd_req_t *req)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);

    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition not found");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Partition not found");
        return ESP_FAIL;
    }

    if (binary_size == 0) {
        return ESP_FAIL;
    }
    // Get the size of the binary
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    httpd_resp_set_hdr(req, "Connection", "close");

    // Complete HEAD response with no body
    return httpd_resp_send(req, NULL, binary_size); // No body for HEAD method
}

static const httpd_uri_t get_root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

static const httpd_uri_t head_root = {
    .uri       = "/",
    .method    = HTTP_HEAD,
    .handler   = root_head_handler

};

esp_err_t example_test_start_webserver(void)
{
    httpd_handle_t server = NULL;
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

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
        ESP_LOGI(TAG, "Error starting server!");
        return ret;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &get_root);
    httpd_register_uri_handler(server, &head_root);
    return ESP_OK;
}
