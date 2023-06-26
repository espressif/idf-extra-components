/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) Co. Ltd.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "download_file.h"

static const char *TAG = "file_downloader";

static esp_err_t download_file_event_handler(esp_http_client_event_t *evt);
static void file_write_task(void *arg);

typedef struct {
    FILE *f_out;
    size_t buffer_size;
    RingbufHandle_t rb;
    SemaphoreHandle_t start;
    SemaphoreHandle_t done;
    bool skip_file_buffer;
    size_t bytes_downloaded;
    size_t bytes_written;
    size_t last_download_percent;
    size_t content_length;
    int64_t download_waiting_for_ringbuf_us;
    int64_t write_waiting_for_sdcard_us;
    void (*progress_cb)(void *user_data, size_t bytes_done, size_t bytes_total); /*!< Callback to call on progress */
    void *user_data;
} download_args_t;


esp_err_t download_file(const char *url, FILE *f_out, const download_file_config_t *config)
{
    esp_err_t ret = ESP_OK;
    TaskHandle_t task_handle = NULL;
    esp_http_client_handle_t client = NULL;

    /* Start the file writing task */
    download_args_t args = {
        .f_out = f_out,
        .buffer_size = config->buffer_size,
        .rb = xRingbufferCreate(config->buffer_size, RINGBUF_TYPE_BYTEBUF),
        .start = xSemaphoreCreateBinary(),
        .done = xSemaphoreCreateBinary(),
        .skip_file_buffer = config->skip_file_buffer,
        .progress_cb = config->progress_cb,
        .user_data = config->user_data,
    };

    esp_http_client_config_t http_client_config = {
        .url = url,
        .event_handler = &download_file_event_handler,
        .user_data = &args,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = &esp_crt_bundle_attach,
#endif
        .buffer_size = config->buffer_size,
        .timeout_ms = config->timeout_ms,
    };

    if (config->http_client_config_cb != NULL) {
        ESP_GOTO_ON_ERROR(config->http_client_config_cb(config->user_data, &http_client_config), out, TAG, "Failed in config callback");
    }

    client = esp_http_client_init(&http_client_config);
    ESP_GOTO_ON_FALSE(client != NULL, ESP_ERR_NO_MEM, out, TAG, "Failed to initialise HTTP client");

    if (config->http_client_post_init_cb != NULL) {
        ESP_GOTO_ON_ERROR(config->http_client_post_init_cb(config->user_data, client), out, TAG, "Failed in post init callback");
    }

    int res = xTaskCreatePinnedToCore(&file_write_task, "download_file_task", 4096, &args, 5, &task_handle, 1);
    ESP_GOTO_ON_FALSE(res == pdPASS, ESP_ERR_NO_MEM, out, TAG, "Failed to create file write task");

    int64_t start = esp_timer_get_time();
    ret = esp_http_client_perform(client);
    int64_t end = esp_timer_get_time();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %"PRIu64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        ESP_LOGI(TAG, "Time taken: %d ms Speed: %.2f kB/sec", (int) (end - start) / 1000, (args.content_length / 1024.0f) / ((end - start) / 1000000.0f));
        ESP_LOGI(TAG, "Download task spent %d ms blocked on writing to ringbuffer", (int) args.download_waiting_for_ringbuf_us / 1000);
        ESP_LOGI(TAG, "File write task spent %d ms blocked on writing to SD card", (int) args.write_waiting_for_sdcard_us / 1000);
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
    }

    xSemaphoreTake(args.done, portMAX_DELAY);

out:
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }
    vRingbufferDelete(args.rb);
    vSemaphoreDelete(args.done);
    return ret;
}


static void file_write_task(void *arg)
{
    download_args_t *args = (download_args_t *) arg;
    args->bytes_written = 0;
    xSemaphoreTake(args->start, portMAX_DELAY);
    if (args->content_length == 0) {
        ESP_LOGE(TAG, "Content length is 0");
        vTaskDelete(NULL);
        return;
    }

    while (args->bytes_written < args->content_length) {
        size_t remaining = 0;
        vRingbufferGetInfo(args->rb, NULL, NULL, NULL, NULL, &remaining);

        size_t to_write = args->buffer_size;
        if (args->bytes_written + remaining == args->content_length) {
            to_write = remaining;
        }
        ESP_LOGD(TAG, "to_write: %d, remaining: %d", to_write, remaining);
        uint8_t *rb_buf = xRingbufferReceive(args->rb, &to_write, pdMS_TO_TICKS(10000));
        if (rb_buf == NULL) {
            ESP_LOGE(TAG, "Failed to read from ringbuffer");
            continue;
        }
        int64_t start = esp_timer_get_time();

        ssize_t written_bytes;
        if (args->skip_file_buffer) {
            written_bytes = write(fileno(args->f_out), rb_buf, to_write);
        } else {
            written_bytes = fwrite(rb_buf, 1, to_write, args->f_out);
        }
        int64_t end = esp_timer_get_time();
        if (written_bytes != to_write) {
            ESP_LOGE(TAG, "Failed to write to file");
            break;
        }
        args->write_waiting_for_sdcard_us += end - start;
        args->bytes_written += written_bytes;
        vRingbufferReturnItem(args->rb, rb_buf);

        ESP_LOGD(TAG, "Downloaded %d, written %d", args->bytes_downloaded, args->bytes_written);
        size_t download_percent = (args->bytes_downloaded * 100) / args->content_length;
        if (download_percent - args->last_download_percent >= 1) {
            if (args->progress_cb != NULL) {
                args->progress_cb(args->user_data, args->content_length, args->bytes_written);
            }
            args->last_download_percent = download_percent;
        }
    }
    if (args->progress_cb != NULL) {
        args->progress_cb(args->user_data, args->content_length, args->bytes_written);
    }
    ESP_LOGI(TAG, "Download done, written %d bytes to file", args->bytes_written);
    xSemaphoreGive(args->done);
    vTaskDelete(NULL);
}

static esp_err_t download_file_event_handler(esp_http_client_event_t *evt)
{
    download_args_t *args = (download_args_t *) evt->user_data;
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        if (strcmp(evt->header_key, "Content-Length") == 0) {
            args->content_length = atoi(evt->header_value);
            ESP_LOGI(TAG, "Content-length: %d", args->content_length);
            // start the file write task
            xSemaphoreGive(args->start);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        args->bytes_downloaded += evt->data_len;
        if (!esp_http_client_is_chunked_response(evt->client)) {

            /* Write out data received in the event */
            int64_t start = esp_timer_get_time();
            xRingbufferSend(args->rb, (void *) evt->data, evt->data_len, portMAX_DELAY);
            int64_t end = esp_timer_get_time();
            args->download_waiting_for_ringbuf_us += end - start;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    default:
        ESP_LOGW(TAG, "Unexpected event id: %d", evt->event_id);
    }
    return ESP_OK;
}
