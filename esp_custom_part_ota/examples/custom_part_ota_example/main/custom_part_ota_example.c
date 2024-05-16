#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "protocol_examples_common.h"
#include "errno.h"
#include "esp_err.h"
#include "esp_custom_part_ota.h"

static const char *TAG = "example";

#define BUFFSIZE 1024
static char ota_write_data[BUFFSIZE + 1] = { 0 };

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define CUSTOM_PARTITION "storage"

static esp_err_t write_into_custom_partition(void)
{
    char *data_to_write = "This is old data";
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, CUSTOM_PARTITION);
    if (!partition) {
        ESP_LOGE(TAG, "Unable to find custom data partition");
        return ESP_FAIL;
    }
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_partition_write(partition, 0, data_to_write, strlen(data_to_write));
    return err;
}

static void read_custom_partition(void)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, CUSTOM_PARTITION);
    if (!partition) {
        ESP_LOGE(TAG, "Unable to find custom data partition");
        return;
    }
    // Reading the first 100 bytes which is enough to see that data has been updated in this example.
    char *data_read = calloc(100, sizeof(char));
    if (!data_read) {
        ESP_LOGE(TAG, "Unable to allocate buffer to read data");
        return;
    }
    esp_err_t ret = esp_partition_read(partition, 0, data_read, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error while reading data from the custom partition: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, data_read, 100, ESP_LOG_INFO);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
    }
}

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void custom_part_ota_example_task(void *pvParameter)
{
    const esp_partition_t *update_partition = NULL;
    esp_err_t err  = ESP_FAIL;

    esp_http_client_config_t config = {
        .url = CONFIG_EXAMPLE_DATA_DOWNLOAD_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, CUSTOM_PARTITION);
    if (!update_partition) {
        ESP_LOGE(TAG, "Failed to get the update partition");
        task_fatal_error();
    }
    esp_custom_part_ota_cfg_t ota_config = {
        .update_partition = update_partition,
    };
    esp_custom_part_ota_handle_t ota_handle = esp_custom_part_ota_begin(ota_config);
    if (!ota_handle) {
        ESP_LOGE(TAG, "Failed to begin OTA update process");
        task_fatal_error();
    }

#if CONFIG_EXAMPLE_PARTITION_BACKUP
    err = esp_custom_part_ota_partition_backup(ota_handle, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to backup the update partition: %s", esp_err_to_name(err));
        task_fatal_error();
    }
#endif // CONFIG_EXAMPLE_PARTITION_BACKUP

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        task_fatal_error();
    }
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        task_fatal_error();
    }
    esp_http_client_fetch_headers(client);
    int data_written = 0;
    while (1) {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            http_cleanup(client);
            task_fatal_error();
        } else if (data_read > 0) {
            err = esp_custom_part_ota_write(ota_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
                http_cleanup(client);
                esp_custom_part_ota_abort(ota_handle);
                task_fatal_error();
            }
            data_written += data_read;
        } else if (data_read == 0) {
            /*
             * As esp_http_client_read never returns negative error code, we rely on
             * `errno` to check for underlying transport connectivity closure if any
             */
            if (errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                break;
            }
            if (esp_http_client_is_complete_data_received(client) == true) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            }
        }
    }
    if (esp_http_client_is_complete_data_received(client) != true) {
        ESP_LOGE(TAG, "Error in receiving complete file");
        http_cleanup(client);
        esp_custom_part_ota_abort(ota_handle);
        task_fatal_error();
    }
    err = esp_custom_part_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_custom_part_ota_end failed (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        task_fatal_error();
    }
    read_custom_partition();
    esp_restart();
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    err = write_into_custom_partition();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write data in custom partition");
    }
    read_custom_partition();

    xTaskCreate(&custom_part_ota_example_task, "custom_part_ota_example_task", 8192, NULL, 5, NULL);
}

