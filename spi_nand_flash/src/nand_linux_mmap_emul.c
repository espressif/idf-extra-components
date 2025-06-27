/*
 * NAND Flash Memory Emulation for Linux Host
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "esp_log.h"
#include "nand_linux_mmap_emul.h"
#include "nand.h"

static const char *TAG = "linux_nandflash";

// Initialize NAND flash Emulation
// Exposes direct pointer to the memory mapped file created by nand_partition_mmap
// No address alignment is performed
static esp_err_t nand_emul_mmap_init(nand_mmap_emul_handle_t *emul_handle)
{
    if (emul_handle->mem_file_buf != NULL) {
        ESP_LOGE(TAG, "NAND flash already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Create or open file
    if (emul_handle->file_mmap_ctrl.flash_file_name[0] != '\0') {
        emul_handle->mem_file_fd = open(emul_handle->file_mmap_ctrl.flash_file_name, O_RDWR | O_CREAT, 0600);
    } else {
        emul_handle->mem_file_fd = mkstemp(emul_handle->file_mmap_ctrl.flash_file_name);
    }

    if (emul_handle->mem_file_fd == -1) {
        ESP_LOGE(TAG, "Failed to open NAND file %s: %s",
                 emul_handle->file_mmap_ctrl.flash_file_name, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }

    // Set file size
    if (ftruncate(emul_handle->mem_file_fd, emul_handle->file_mmap_ctrl.flash_file_size) != 0) {
        ESP_LOGE(TAG, "Failed to set NAND file size: %s", strerror(errno));
        close(emul_handle->mem_file_fd);
        return ESP_ERR_INVALID_SIZE;
    }

    // Map file to memory
    emul_handle->mem_file_buf = mmap(NULL, emul_handle->file_mmap_ctrl.flash_file_size,
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     emul_handle->mem_file_fd, 0);
    if (emul_handle->mem_file_buf == MAP_FAILED) {
        ESP_LOGE(TAG, "Failed to mmap NAND file: %s", strerror(errno));
        close(emul_handle->mem_file_fd);
        return ESP_ERR_NO_MEM;
    }

    // Initialize with 0xFF (erased state)
    memset(emul_handle->mem_file_buf, 0xFF, emul_handle->file_mmap_ctrl.flash_file_size);

    ESP_LOGI(TAG, "NAND flash emulation initialized: %s (size: %zu bytes)",
             emul_handle->file_mmap_ctrl.flash_file_name,
             emul_handle->file_mmap_ctrl.flash_file_size);

    return ESP_OK;
}

// Cleanup NAND flash emulation
static esp_err_t nand_emul_mmap_deinit(nand_mmap_emul_handle_t *emul_handle)
{
    if (emul_handle->mem_file_buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Unmap memory
    if (munmap(emul_handle->mem_file_buf, emul_handle->file_mmap_ctrl.flash_file_size) != 0) {
        ESP_LOGE(TAG, "Failed to munmap NAND file: %s", strerror(errno));
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Close file
    if (close(emul_handle->mem_file_fd) != 0) {
        ESP_LOGE(TAG, "Failed to close NAND file: %s", strerror(errno));
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Remove file if requested
    if (!emul_handle->file_mmap_ctrl.keep_dump) {
        if (remove(emul_handle->file_mmap_ctrl.flash_file_name) != 0) {
            ESP_LOGE(TAG, "Failed to remove NAND file: %s", strerror(errno));
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    // Reset state
    emul_handle->mem_file_buf = NULL;
    emul_handle->mem_file_fd = -1;
    memset(&emul_handle->file_mmap_ctrl, 0, sizeof(emul_handle->file_mmap_ctrl));

    return ESP_OK;
}

esp_err_t nand_emul_init(spi_nand_flash_device_t *handle, nand_file_mmap_emul_config_t *cfg)
{
    nand_mmap_emul_handle_t *emul_handle = calloc(1, sizeof(nand_mmap_emul_handle_t));
    if (emul_handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    emul_handle->mem_file_buf = NULL;
    emul_handle->mem_file_fd = -1;
#ifdef CONFIG_NAND_ENABLE_STATS
    emul_handle->stats.read_ops = 0;
    emul_handle->stats.write_ops = 0;
    emul_handle->stats.erase_ops = 0;
    emul_handle->stats.read_bytes = 0;
    emul_handle->stats.write_bytes = 0;
#endif //CONFIG_NAND_ENABLE_STATS
    handle->emul_handle = emul_handle;

    // Store configuration
    if (!cfg->flash_file_size) {
        cfg->flash_file_size = EMULATED_NAND_SIZE;
    }
    strlcpy(emul_handle->file_mmap_ctrl.flash_file_name,
            *(cfg->flash_file_name) ? cfg->flash_file_name : "/tmp/idf-nand-XXXXXX",
            sizeof(emul_handle->file_mmap_ctrl.flash_file_name));
    emul_handle->file_mmap_ctrl.flash_file_size = cfg->flash_file_size ? cfg->flash_file_size : EMULATED_NAND_SIZE;
    emul_handle->file_mmap_ctrl.keep_dump = cfg->keep_dump;

    return nand_emul_mmap_init(emul_handle);
}

esp_err_t nand_emul_deinit(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = nand_emul_mmap_deinit(handle->emul_handle);
    free(handle->emul_handle);
    return ret;
}

// Read from NAND
esp_err_t nand_emul_read(spi_nand_flash_device_t *handle, size_t addr, void *dst, size_t size)
{
    nand_mmap_emul_handle_t *emul_handle = handle->emul_handle;
    if (emul_handle->mem_file_buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (addr + size > emul_handle->file_mmap_ctrl.flash_file_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    void *src_addr = emul_handle->mem_file_buf + addr;
    memcpy(dst, src_addr, size);

#ifdef CONFIG_NAND_ENABLE_STATS
    emul_handle->stats.read_ops++;
    emul_handle->stats.read_bytes += size;
#endif

    return ESP_OK;
}

// Write to NAND
esp_err_t nand_emul_write(spi_nand_flash_device_t *handle, size_t addr, const void *src, size_t size)
{
    nand_mmap_emul_handle_t *emul_handle = handle->emul_handle;
    if (emul_handle->mem_file_buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (addr + size > emul_handle->file_mmap_ctrl.flash_file_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    void *dst_addr = emul_handle->mem_file_buf + addr;

    // Emulate NAND behavior: can only change 1->0
    for (size_t i = 0; i < size; i++) {
        ((uint8_t *)dst_addr)[i] &= ((uint8_t *)src)[i];
    }

#ifdef CONFIG_NAND_ENABLE_STATS
    emul_handle->stats.write_ops++;
    emul_handle->stats.write_bytes += size;
#endif

    return ESP_OK;
}

// Erase NAND memory range
esp_err_t nand_emul_erase_block(spi_nand_flash_device_t *handle, size_t offset)
{
    nand_mmap_emul_handle_t *emul_handle = handle->emul_handle;
    if (emul_handle->mem_file_buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (offset + handle->chip.block_size > emul_handle->file_mmap_ctrl.flash_file_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    void *dst_addr = emul_handle->mem_file_buf + offset;
    memset(dst_addr, 0xFF,  handle->chip.block_size);

#ifdef CONFIG_NAND_ENABLE_STATS
    emul_handle->stats.erase_ops++;
#endif

    return ESP_OK;
}

#ifdef CONFIG_NAND_ENABLE_STATS
// Clear statistics
void nand_emul_clear_stats(spi_nand_flash_device_t *handle)
{
    nand_mmap_emul_handle_t *emul_handle = handle->emul_handle;
    emul_handle->stats.read_ops = 0;
    emul_handle->stats.write_ops = 0;
    emul_handle->stats.erase_ops = 0;
    emul_handle->stats.read_bytes = 0;
    emul_handle->stats.write_bytes = 0;
}
#endif
