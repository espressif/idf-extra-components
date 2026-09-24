/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include <sys/lock.h>
#include "dhara/nand.h"
#include "dhara/map.h"
#include "dhara/journal.h"
#include "dhara/error.h"
#include "esp_check.h"
#include "esp_err.h"
#ifndef CONFIG_IDF_TARGET_LINUX
#include "spi_nand_oper.h"
#endif
#include "nand_impl.h"
#include "nand.h"
#include "nand_device_types.h"

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "esp_nand_blockdev.h"
#endif

#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
#define DHARA_META_CACHE_ENTRY_COUNT CONFIG_NAND_FLASH_DHARA_META_CACHE_SLOTS
#endif

#ifdef CONFIG_NAND_FLASH_PERF_STATS
#define PERF_STATS_INC(priv, counter) ((priv)->perf_stats.counter++)
#else
#define PERF_STATS_INC(priv, counter) ((void)(priv))
#endif

#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
typedef struct {
    dhara_page_t page;
    size_t offset;
    size_t length;
    uint8_t data[DHARA_META_SIZE];
    bool valid;
} dhara_meta_cache_entry_t;
#endif

typedef struct {
    struct dhara_nand dhara_nand;
    struct dhara_map dhara_map;
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    esp_blockdev_handle_t bdl_handle;
#endif
    spi_nand_flash_device_t *parent_handle;
#ifdef CONFIG_NAND_FLASH_PERF_STATS
    spi_nand_flash_perf_stats_t perf_stats;
#endif
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    dhara_meta_cache_entry_t meta_cache[DHARA_META_CACHE_ENTRY_COUNT];
    uint8_t next_meta_cache_entry;
#endif
} spi_nand_flash_dhara_priv_data_t;

#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
static void meta_cache_invalidate_all(spi_nand_flash_dhara_priv_data_t *priv)
{
    for (size_t i = 0; i < DHARA_META_CACHE_ENTRY_COUNT; i++) {
        priv->meta_cache[i].valid = false;
    }
    priv->next_meta_cache_entry = 0;
}

static void meta_cache_invalidate_page(spi_nand_flash_dhara_priv_data_t *priv, dhara_page_t page)
{
    for (size_t i = 0; i < DHARA_META_CACHE_ENTRY_COUNT; i++) {
        if (priv->meta_cache[i].valid && priv->meta_cache[i].page == page) {
            priv->meta_cache[i].valid = false;
        }
    }
}

static void meta_cache_invalidate_block(spi_nand_flash_dhara_priv_data_t *priv, dhara_block_t block)
{
    const dhara_page_t first_page = block << priv->dhara_nand.log2_ppb;
    const dhara_page_t page_count = 1u << priv->dhara_nand.log2_ppb;
    for (size_t i = 0; i < DHARA_META_CACHE_ENTRY_COUNT; i++) {
        if (priv->meta_cache[i].valid &&
                priv->meta_cache[i].page >= first_page &&
                priv->meta_cache[i].page < first_page + page_count) {
            priv->meta_cache[i].valid = false;
        }
    }
}

static bool meta_cache_read(spi_nand_flash_dhara_priv_data_t *priv, dhara_page_t page,
                            size_t offset, size_t length, uint8_t *data)
{
    for (size_t i = 0; i < DHARA_META_CACHE_ENTRY_COUNT; i++) {
        const dhara_meta_cache_entry_t *entry = &priv->meta_cache[i];
        if (entry->valid && entry->page == page &&
                entry->offset == offset && entry->length == length) {
            memcpy(data, entry->data, DHARA_META_SIZE);
            return true;
        }
    }
    return false;
}

static void meta_cache_store(spi_nand_flash_dhara_priv_data_t *priv, dhara_page_t page,
                             size_t offset, size_t length, const uint8_t *data)
{
    dhara_meta_cache_entry_t *entry = &priv->meta_cache[priv->next_meta_cache_entry];
    entry->valid = false;
    entry->page = page;
    entry->offset = offset;
    entry->length = length;
    memcpy(entry->data, data, DHARA_META_SIZE);
    entry->valid = true;
    priv->next_meta_cache_entry = (priv->next_meta_cache_entry + 1) % DHARA_META_CACHE_ENTRY_COUNT;
}
#endif

static esp_err_t dhara_init(spi_nand_flash_device_t *handle, void *bdl_handle)
{
    // create a holder structure for dhara context
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = heap_caps_calloc(1, sizeof(spi_nand_flash_dhara_priv_data_t), MALLOC_CAP_DEFAULT);
    if (dhara_priv_data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->ops_priv_data = dhara_priv_data;
    // store the pointer back to device structure in the holder structure
    dhara_priv_data->parent_handle = handle;
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    dhara_priv_data->bdl_handle = (esp_blockdev_handle_t)bdl_handle;
#endif

    dhara_priv_data->dhara_nand.log2_page_size = handle->chip.log2_page_size;
    dhara_priv_data->dhara_nand.log2_ppb = handle->chip.log2_ppb;
    dhara_priv_data->dhara_nand.num_blocks = handle->chip.num_blocks;

    dhara_map_init(&dhara_priv_data->dhara_map, &dhara_priv_data->dhara_nand, handle->work_buffer, handle->config.gc_factor);
    dhara_error_t ignored;
    dhara_map_resume(&dhara_priv_data->dhara_map, &ignored);

    return ESP_OK;
}

static esp_err_t dhara_deinit(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    // clear dhara map
    dhara_map_init(&dhara_priv_data->dhara_map, &dhara_priv_data->dhara_nand, handle->work_buffer, handle->config.gc_factor);
    dhara_map_clear(&dhara_priv_data->dhara_map);
    return ESP_OK;
}

static esp_err_t dhara_read(spi_nand_flash_device_t *handle, uint8_t *buffer, dhara_sector_t sector_id)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_read(&dhara_priv_data->dhara_map, sector_id, buffer, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_write(spi_nand_flash_device_t *handle, const uint8_t *buffer, dhara_sector_t sector_id)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_write(&dhara_priv_data->dhara_map, sector_id, buffer, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_copy_sector(spi_nand_flash_device_t *handle, dhara_sector_t src_sec, dhara_sector_t dst_sec)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_copy_sector(&dhara_priv_data->dhara_map, src_sec, dst_sec, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_trim(spi_nand_flash_device_t *handle, dhara_sector_t sector_id)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_trim(&dhara_priv_data->dhara_map, sector_id, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_sync(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_sync(&dhara_priv_data->dhara_map, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_get_capacity(spi_nand_flash_device_t *handle, dhara_sector_t *number_of_sectors)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    *number_of_sectors = dhara_map_capacity(&dhara_priv_data->dhara_map);
    return ESP_OK;
}

static esp_err_t dhara_gc(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_gc(&dhara_priv_data->dhara_map, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_erase_chip(spi_nand_flash_device_t *handle)
{
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    spi_nand_flash_dhara_priv_data_t *priv = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    meta_cache_invalidate_all(priv);
#endif
    return nand_erase_chip(handle);
}

static esp_err_t dhara_erase_block(spi_nand_flash_device_t *handle, uint32_t block)
{
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    spi_nand_flash_dhara_priv_data_t *priv = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    meta_cache_invalidate_block(priv, block);
#endif
    return nand_erase_block(handle, block);
}

const spi_nand_ops dhara_nand_ops = {
    .init = &dhara_init,
    .deinit = &dhara_deinit,
    .read = &dhara_read,
    .write = &dhara_write,
    .erase_chip = &dhara_erase_chip,
    .erase_block = &dhara_erase_block,
    .trim = &dhara_trim,
    .sync = &dhara_sync,
    .copy_sector = &dhara_copy_sector,
    .get_capacity = &dhara_get_capacity,
    .gc = &dhara_gc,
};

esp_err_t nand_wl_attach_ops(spi_nand_flash_device_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->ops = &dhara_nand_ops;
    return ESP_OK;
}

esp_err_t nand_wl_detach_ops(spi_nand_flash_device_t *handle)
{
    free(handle->ops_priv_data);
    handle->ops_priv_data = NULL;
    handle->ops = NULL;
    return ESP_OK;
}

esp_err_t nand_wl_get_perf_stats(spi_nand_flash_device_t *handle, spi_nand_flash_perf_stats_t *stats)
{
    if (handle == NULL || stats == NULL || handle->ops_priv_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#ifdef CONFIG_NAND_FLASH_PERF_STATS
    spi_nand_flash_dhara_priv_data_t *priv = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    *stats = priv->perf_stats;
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t nand_wl_reset_perf_stats(spi_nand_flash_device_t *handle)
{
    if (handle == NULL || handle->ops_priv_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#ifdef CONFIG_NAND_FLASH_PERF_STATS
    spi_nand_flash_dhara_priv_data_t *priv = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    memset(&priv->perf_stats, 0, sizeof(priv->perf_stats));
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t nand_wl_invalidate_metadata_cache(spi_nand_flash_device_t *handle)
{
    if (handle == NULL || handle->ops_priv_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    meta_cache_invalidate_all((spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data);
#endif
    return ESP_OK;
}

/*------------------------------------------------------------------------------------------------------*/

// The following APIs are implementations required by the Dhara library.
// Please refer to the header file dhara/nand.h for details.

int dhara_nand_read(const struct dhara_nand *n, dhara_page_t p, size_t offset, size_t length,
                    uint8_t *data, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = NULL;
    esp_err_t ret = ESP_OK;
    if (length == DHARA_META_SIZE) {
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
        if (meta_cache_read(dhara_priv_data, p, offset, length, data)) {
            PERF_STATS_INC(dhara_priv_data, metadata_cache_hits);
            return 0;
        }
#endif
        PERF_STATS_INC(dhara_priv_data, metadata_cache_misses);
    }
    PERF_STATS_INC(dhara_priv_data, physical_reads);
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    dev_handle = (spi_nand_flash_device_t *)bdl_handle->ctx;
    ret = bdl_handle->ops->read(bdl_handle, data, length,
                                (p * bdl_handle->geometry.read_size) + offset, length);
#else
    dev_handle = dhara_priv_data->parent_handle;
    ret = nand_read(dev_handle, p, offset, length, data);
#endif
    if (ret != ESP_OK) {
        if (dev_handle->chip.ecc_data.ecc_corrected_bits_status == NAND_ECC_NOT_CORRECTED) {
            dhara_set_error(err, DHARA_E_ECC);
        }
        return -1;
    }
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    if (length == DHARA_META_SIZE) {
        meta_cache_store(dhara_priv_data, p, offset, length, data);
    }
#endif
    return 0;
}

int dhara_nand_prog(const struct dhara_nand *n, dhara_page_t p, const uint8_t *data, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    meta_cache_invalidate_page(dhara_priv_data, p);
#endif
    PERF_STATS_INC(dhara_priv_data, physical_programs);
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    ret = bdl_handle->ops->write(bdl_handle, data, (p * bdl_handle->geometry.write_size),
                                 bdl_handle->geometry.write_size);
#else
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    ret = nand_prog(dev_handle, p, data);
#endif
    if (ret) {
        if (ret == ESP_ERR_NOT_FINISHED) {
            dhara_set_error(err, DHARA_E_BAD_BLOCK);
        }
        return -1;
    }
    return 0;
}

int dhara_nand_erase(const struct dhara_nand *n, dhara_block_t b, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    meta_cache_invalidate_block(dhara_priv_data, b);
#endif
    PERF_STATS_INC(dhara_priv_data, physical_erases);
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    ret = bdl_handle->ops->erase(bdl_handle, b * bdl_handle->geometry.erase_size,
                                 bdl_handle->geometry.erase_size);
#else
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    ret = nand_erase_block(dev_handle, b);
#endif
    if (ret) {
        if (ret == ESP_ERR_NOT_FINISHED) {
            dhara_set_error(err, DHARA_E_BAD_BLOCK);
        }
        return -1;
    }
    return 0;
}

int dhara_nand_is_bad(const struct dhara_nand *n, dhara_block_t b)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    bool is_bad_status = false;
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    esp_blockdev_cmd_arg_is_bad_block_t bad_block_status = {b, false};
    ret = bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_block_status);
    is_bad_status = bad_block_status.status;
#else
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    ret = nand_is_bad(dev_handle, b, &is_bad_status);
#endif
    if (ret || is_bad_status == true) {
        return 1;
    }
    return 0;
}

void dhara_nand_mark_bad(const struct dhara_nand *n, dhara_block_t b)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    meta_cache_invalidate_block(dhara_priv_data, b);
#endif
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    uint32_t block = b;
    bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK, &block);
#else
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    nand_mark_bad(dev_handle, b);
#endif
    return;
}

int dhara_nand_is_free(const struct dhara_nand *n, dhara_page_t p)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    bool is_free_status = true;
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    esp_blockdev_cmd_arg_is_free_page_t page_free_status = {p, true};
    ret = bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status);
    is_free_status = page_free_status.status;
#else
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    ret = nand_is_free(dev_handle, p, &is_free_status);
#endif

    if (ret != ESP_OK) {
        return 0;
    }
    if (is_free_status == true) {
        return 1;
    }
    return 0;
}

int dhara_nand_copy(const struct dhara_nand *n, dhara_page_t src, dhara_page_t dst, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = NULL;
    esp_err_t ret = ESP_OK;

#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    meta_cache_invalidate_page(dhara_priv_data, dst);
#endif
    PERF_STATS_INC(dhara_priv_data, physical_copies);
#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
    assert(dhara_priv_data->bdl_handle != NULL);
    esp_blockdev_handle_t bdl_handle = dhara_priv_data->bdl_handle;
    dev_handle = (spi_nand_flash_device_t *)bdl_handle->ctx;
    esp_blockdev_cmd_arg_copy_page_t copy_arg = {
        .src_page = src,
        .dst_page = dst
    };
    ret = dhara_priv_data->bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_COPY_PAGE, &copy_arg);
#else
    dev_handle = dhara_priv_data->parent_handle;
    ret = nand_copy(dev_handle, src, dst);
#endif
    if (ret) {
        if (dev_handle->chip.ecc_data.ecc_corrected_bits_status == NAND_ECC_NOT_CORRECTED) {
            dhara_set_error(err, DHARA_E_ECC);
        }
        if (ret == ESP_ERR_NOT_FINISHED) {
            dhara_set_error(err, DHARA_E_BAD_BLOCK);
        }
        return -1;
    }
    return 0;
}
