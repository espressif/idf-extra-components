/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "nand.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"
#include "nand_device_types.h"

static const char *TAG = "nand_wl_blockdev";

/**************************************************************************************
 **************************************************************************************
 * Block Device Layer interface implementation
 **************************************************************************************
 */

static esp_err_t spi_nand_flash_wl_blockdev_read(esp_blockdev_handle_t handle, uint8_t *dst_buf, size_t dst_buf_size, uint64_t src_addr, size_t data_read_len)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)((esp_blockdev_handle_t)handle->ctx)->ctx;
    uint32_t page_size = handle->geometry.read_size;

    if (page_size == 0) {
        ESP_LOGE(TAG, "Invalid page size (0)");
        return ESP_ERR_INVALID_SIZE;
    }

    // Check alignment
    if ((src_addr % page_size) != 0) {
        ESP_LOGE(TAG, "Source address 0x%llx not aligned to page size %u", src_addr, page_size);
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t page_id = (uint32_t)(src_addr >> dev_handle->chip.log2_page_size);
    esp_err_t res = spi_nand_flash_read_page(dev_handle, dst_buf, page_id);
    return res;
}

static esp_err_t spi_nand_flash_wl_blockdev_write(esp_blockdev_handle_t handle, const uint8_t *src_buf, uint64_t dst_addr, size_t data_write_len)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)((esp_blockdev_handle_t)handle->ctx)->ctx;
    uint32_t page_size = handle->geometry.write_size;

    if (page_size == 0) {
        ESP_LOGE(TAG, "Invalid page size (0)");
        return ESP_ERR_INVALID_SIZE;
    }

    // Check alignment
    if ((dst_addr % page_size) != 0) {
        ESP_LOGE(TAG, "Write address 0x%llx not aligned to page size %u", dst_addr, page_size);
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t page_id = (uint32_t)(dst_addr >> dev_handle->chip.log2_page_size);
    esp_err_t res = spi_nand_flash_write_page(dev_handle, src_buf, page_id);
    return res;
}

static esp_err_t spi_nand_flash_wl_blockdev_erase(esp_blockdev_handle_t handle, uint64_t start_addr, size_t erase_len)
{
    esp_err_t res = ESP_OK;
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)((esp_blockdev_handle_t)handle->ctx)->ctx;
    uint32_t page_size = handle->geometry.write_size;

    if (page_size == 0) {
        ESP_LOGE(TAG, "Invalid page size (0)");
        return ESP_ERR_INVALID_SIZE;
    }

    // Check alignment
    if ((start_addr % page_size) != 0) {
        ESP_LOGE(TAG, "Start address 0x%llx not aligned to page size %u", start_addr, page_size);
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t page_count = (uint32_t)(erase_len >> dev_handle->chip.log2_page_size);
    uint32_t start_page = (uint32_t)(start_addr >> dev_handle->chip.log2_page_size);
    for (uint32_t page_id = start_page; page_id < start_page + page_count; page_id++) {
        res = spi_nand_flash_trim(dev_handle, page_id);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "%s, Failed to trim the page", __func__);
            return res;
        }
    }
    res = spi_nand_flash_gc(dev_handle);
    return res;
}

static esp_err_t spi_nand_flash_wl_blockdev_sync(esp_blockdev_handle_t handle)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)((esp_blockdev_handle_t)handle->ctx)->ctx;
    esp_err_t res = spi_nand_flash_sync(dev_handle);
    return res;
}

static esp_err_t spi_nand_flash_wl_blockdev_ioctl(esp_blockdev_handle_t handle, const uint8_t cmd, void *args)
{
    if (handle == NULL || args == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;

    esp_blockdev_handle_t nand_bdl = (esp_blockdev_handle_t)handle->ctx;
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)nand_bdl->ctx;

    switch (cmd) {
    case ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS: {
        uint32_t *num_pages = (uint32_t *)args;
        ret = spi_nand_flash_get_page_count(dev_handle, num_pages);
    }
    break;

    case ESP_BLOCKDEV_CMD_TRIM_SECTOR: {
        uint32_t *page_id = (uint32_t *)args;
        ret = spi_nand_flash_trim(dev_handle, *page_id);
    }

    break;
    case ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO:
    case ESP_BLOCKDEV_CMD_GET_BAD_BLOCKS_COUNT:
    case ESP_BLOCKDEV_CMD_GET_ECC_STATS: {
        ret = nand_bdl->ops->ioctl(nand_bdl, cmd, args);
    }
    break;

    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ret;
}

static esp_err_t spi_nand_flash_wl_blockdev_release(esp_blockdev_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_blockdev_handle_t nand_handle = (esp_blockdev_handle_t)handle->ctx;
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)nand_handle->ctx;

#ifdef CONFIG_IDF_TARGET_LINUX
    ret = nand_emul_deinit(dev_handle);
#endif
    nand_wl_detach_ops(dev_handle);
    nand_handle->ops->release(nand_handle);
    free(handle);

    return ret;
}

static const esp_blockdev_ops_t spi_nand_flash_wl_blockdev_ops = {
    .read = spi_nand_flash_wl_blockdev_read,
    .write = spi_nand_flash_wl_blockdev_write,
    .erase = spi_nand_flash_wl_blockdev_erase,
    .ioctl = spi_nand_flash_wl_blockdev_ioctl,
    .sync = spi_nand_flash_wl_blockdev_sync,
    .release = spi_nand_flash_wl_blockdev_release,
};

esp_err_t spi_nand_flash_wl_get_blockdev(esp_blockdev_handle_t nand_bdl, esp_blockdev_handle_t *out_bdl_handle_ptr)
{
    // Validate input parameters
    ESP_RETURN_ON_FALSE(nand_bdl != NULL, ESP_ERR_INVALID_ARG, TAG, "nand_bdl cannot be NULL");
    ESP_RETURN_ON_FALSE(out_bdl_handle_ptr != NULL, ESP_ERR_INVALID_ARG, TAG, "out_bdl_handle_ptr cannot be NULL");

    spi_nand_flash_device_t *dev = (spi_nand_flash_device_t *)nand_bdl->ctx;
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "spi_nand_flash_device_t pointer cannot be NULL");

    // Validate that Flash BDL operations are available
    ESP_RETURN_ON_FALSE(nand_bdl->ops != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL ops cannot be NULL");
    ESP_RETURN_ON_FALSE(nand_bdl->ops->read != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL read operation is required");
    ESP_RETURN_ON_FALSE(nand_bdl->ops->write != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL write operation is required");
    ESP_RETURN_ON_FALSE(nand_bdl->ops->erase != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL erase operation is required");
    ESP_RETURN_ON_FALSE(nand_bdl->ops->ioctl != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL ioctl operation is required");
    ESP_RETURN_ON_FALSE(nand_bdl->ops->release != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL release operation is required");

    // Initialize ops with dhara operations
    esp_err_t ret = nand_wl_attach_ops(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach wear-leveling operations");
        return ret;
    }

    // Initialize dhara lib
    if (dev->ops->init == NULL) {
        ESP_LOGE(TAG, "Failed to initialize spi_nand_ops");
        return  ESP_FAIL;
    }
    dev->ops->init(dev, nand_bdl);

    // Create wl blockdev
    esp_blockdev_t *blockdev = (esp_blockdev_t *) heap_caps_calloc(1, sizeof(esp_blockdev_t), MALLOC_CAP_DEFAULT);
    if (blockdev == NULL) {
        return ESP_ERR_NO_MEM;
    }
    blockdev->ctx = (void *)nand_bdl;

    ESP_BLOCKDEV_FLAGS_INST_CONFIG_DEFAULT(blockdev->device_flags);
    blockdev->ops = &spi_nand_flash_wl_blockdev_ops;

    // Set up geometry information (BDL exposes logical pages as "sectors" per esp_blockdev API)
    uint32_t num_pages;
    spi_nand_flash_get_page_count(dev, &num_pages);
    uint32_t page_size = dev->chip.page_size;
    uint32_t block_size = dev->chip.block_size;

    blockdev->geometry.disk_size = num_pages * page_size;
    blockdev->geometry.write_size = page_size;
    blockdev->geometry.read_size = page_size;
    blockdev->geometry.erase_size = block_size;
    blockdev->geometry.recommended_write_size = page_size;
    blockdev->geometry.recommended_read_size = page_size;
    blockdev->geometry.recommended_erase_size = block_size;

    *out_bdl_handle_ptr = blockdev;
    return ESP_OK;
}
