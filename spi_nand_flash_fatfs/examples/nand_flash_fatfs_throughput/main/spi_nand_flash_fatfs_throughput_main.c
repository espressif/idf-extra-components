/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Testing-only example: FatFs write/read throughput on SPI NAND (legacy
 * path). Runs four phases over a chunk-size sweep:
 *   1. stdio, default (block) buffering       - "naive fopen/fwrite" cost
 *   2. POSIX read/write, sequential            - raw syscall path
 *   3. POSIX pwrite/pread, randomized order     - random-access pattern
 *   4. POSIX read/write, multiple small files   - directory/fragmentation cost
 * Each phase optionally repeats CONFIG_EXAMPLE_TEST_TRIES times and reports
 * mean/min/max throughput plus driver-level physical op counters and derived
 * write/read amplification. Compare with nand_flash_debug_app for raw
 * page-level (no FatFs/Dhara) numbers.
 *
 * All phases run back-to-back on the same mounted volume, so later phases
 * see whatever physical wear/GC state earlier phases (and the optional
 * preconditioning fill file) left behind. Keep this in mind when comparing
 * phases against each other - they are not measured from identical starting
 * states.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_vfs_fat_nand.h"
#include "nand_diag_api.h"
#include "soc/spi_pins.h"

#ifndef CONFIG_NAND_FLASH_PERF_STATS
#error "This example needs CONFIG_NAND_FLASH_PERF_STATS=y (set in sdkconfig.defaults)"
#endif

/* Edit these to match board / chip limits when comparing SPI link speed. */
#define EXAMPLE_FLASH_FREQ_KHZ      80000
#define EXAMPLE_IO_MODE             SPI_NAND_IO_MODE_QIO

#define EXAMPLE_TEST_FILE_SIZE      (2 * 1024 * 1024)
/* Larger cluster → fewer FAT walks on sequential I/O (reformat required). */
#define EXAMPLE_ALLOC_UNIT_SIZE     (32 * 1024)
#define EXAMPLE_TEST_FILE_PATH      "/nandflash/tp.bin"

/* Multi-file (fragmentation) phase: total bytes are a percentage of the
 * volume's free space at test start, split evenly across this many files,
 * so the test scales with whatever chip/partition is actually attached
 * instead of a size hardcoded for one chip.
 *
 * This phase runs after three other phases have already written tens of
 * MB of logical data (more physical, after write amplification) with no
 * re-erase in between, so it's the most likely phase to run into FTL/GC
 * pressure. FREE_PERCENT is kept low (1%) specifically to bound this
 * phase's own contribution - across the 4-size chunk sweep this is
 * ~4x FREE_PERCENT% of free space in total physical-write volume, since
 * each chunk size re-runs the full per-file byte count. Raise it if you
 * want a more thorough fragmentation test and can tolerate more wear. */
#define EXAMPLE_MULTIFILE_COUNT         10
#define EXAMPLE_MULTIFILE_FREE_PERCENT  1

static const char *TAG = "fatfs_tp";

#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID  SPI3_HOST
#define PIN_MOSI SPI3_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI3_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI3_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI3_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI3_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI3_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else
#define HOST_ID  SPI2_HOST
#define PIN_MOSI SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

static const char *base_path = "/nandflash";

/* NAND page size in bytes, queried once at startup; used to turn physical
 * op counters into a write/read amplification ratio in log_perf_stats(). */
static uint32_t s_page_size = 0;

/* Per-file byte target for the multi-file phase, computed at runtime from
 * actual free space (see app_main) so it scales with the attached chip. */
static size_t s_multifile_per_file_bytes = 0;

/* Per-instance RAM cost of each optional cache, for the overview log only.
 * These are approximate: they mirror the field sizes added to the relevant
 * structs (struct dhara_map, spi_nand_flash_device_t, dhara_meta_cache_entry_t)
 * with typical 32-bit alignment/padding, but the true cost depends on the
 * compiler and target. See each option's Kconfig help for the source figures.
 */
#define EXAMPLE_DHARA_PATH_CACHE_BYTES      140  /* struct dhara_map growth (dhara/Kconfig) */
#define EXAMPLE_PAGE_REG_CACHE_BYTES        8    /* last_loaded_page(4) + last_loaded_status(1) + nand_page_cache_valid(1), padded */
#define EXAMPLE_META_CACHE_BYTES_PER_SLOT   148  /* page(4) + offset(4) + length(4) + data[132] + valid(1), padded */

static void print_cache_kconfig_overview(void)
{
    ESP_LOGI(TAG, "Cache configuration overview:");

#if CONFIG_DHARA_MAP_PATH_CACHE
    ESP_LOGI(TAG, "  DHARA_MAP_PATH_CACHE:            ON  (+%d B)", EXAMPLE_DHARA_PATH_CACHE_BYTES);
#else
    ESP_LOGI(TAG, "  DHARA_MAP_PATH_CACHE:            off");
#endif

#ifdef CONFIG_NAND_FLASH_PAGE_REGISTER_CACHE
    ESP_LOGI(TAG, "  NAND_FLASH_PAGE_REGISTER_CACHE:  ON  (+%d B)", EXAMPLE_PAGE_REG_CACHE_BYTES);
#else
    ESP_LOGI(TAG, "  NAND_FLASH_PAGE_REGISTER_CACHE:  off");
#endif

#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    ESP_LOGI(TAG, "  NAND_FLASH_DHARA_META_CACHE:     ON  (+%d B, %d slots x %d B)",
             EXAMPLE_META_CACHE_BYTES_PER_SLOT * CONFIG_NAND_FLASH_DHARA_META_CACHE_SLOTS,
             CONFIG_NAND_FLASH_DHARA_META_CACHE_SLOTS, EXAMPLE_META_CACHE_BYTES_PER_SLOT);
#else
    ESP_LOGI(TAG, "  NAND_FLASH_DHARA_META_CACHE:     off");
#endif
}

static const size_t s_chunk_sizes[] = {
    512,
    4 * 1024,
    16 * 1024,
    EXAMPLE_ALLOC_UNIT_SIZE,
};

static const char *io_mode_str(spi_nand_flash_io_mode_t mode)
{
    switch (mode) {
    case SPI_NAND_IO_MODE_SIO:
        return "SIO";
    case SPI_NAND_IO_MODE_DOUT:
        return "DOUT";
    case SPI_NAND_IO_MODE_DIO:
        return "DIO";
    case SPI_NAND_IO_MODE_QOUT:
        return "QOUT";
    case SPI_NAND_IO_MODE_QIO:
        return "QIO";
    default:
        return "?";
    }
}

static void example_init_nand_flash(spi_nand_flash_device_t **out_handle, spi_device_handle_t *spi_handle)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        /* Field added in IDF 5.4 (esp_driver_spi); this component's legacy
         * path supports IDF 5.0+, so guard it to keep older IDF buildable. */
        .data_io_default_level = 1,
#endif
    };

    ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    /* Half-duplex required for DIO/DOUT/QIO/QOUT. */
    const uint32_t spi_flags = SPI_DEVICE_HALFDUPLEX;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = EXAMPLE_FLASH_FREQ_KHZ * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = spi_flags,
    };

    spi_device_handle_t spi;
    ESP_ERROR_CHECK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .io_mode = EXAMPLE_IO_MODE,
        .flags = spi_flags,
    };
    assert(devcfg.flags == nand_flash_config.flags);

    spi_nand_flash_device_t *nand_flash_device_handle;
    ESP_ERROR_CHECK(spi_nand_flash_init_device(&nand_flash_config, &nand_flash_device_handle));

    *out_handle = nand_flash_device_handle;
    *spi_handle = spi;
}

static void example_deinit_nand_flash(spi_nand_flash_device_t *flash, spi_device_handle_t spi)
{
    ESP_ERROR_CHECK(spi_nand_flash_deinit_device(flash));
    ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    ESP_ERROR_CHECK(spi_bus_free(HOST_ID));
}

/* Fisher-Yates shuffle of a 0..count-1 index array, used by the random-order
 * phase. rand()/RAND_MAX is plenty for the chunk counts this example uses
 * (at most a few thousand); this is a test tool, not a security context. */
static void shuffle_indices(size_t *order, size_t count)
{
    for (size_t i = count; i > 1; i--) {
        size_t j = (size_t)(rand() % (int)i);
        size_t tmp = order[i - 1];
        order[i - 1] = order[j];
        order[j] = tmp;
    }
}

/* Accumulates per-try throughput (kB/s) so a repeated phase can report
 * mean/min/max instead of a single sample. */
typedef struct {
    double sum_kbps;
    double min_kbps;
    double max_kbps;
    int count;
} timing_acc_t;

static void timing_acc_init(timing_acc_t *acc)
{
    acc->sum_kbps = 0.0;
    acc->min_kbps = 1e18;
    acc->max_kbps = 0.0;
    acc->count = 0;
}

static void timing_acc_add(timing_acc_t *acc, size_t bytes, int64_t us)
{
    double kbps = (us > 0) ? ((double)bytes / (double)us * 1000.0) : 0.0;
    acc->sum_kbps += kbps;
    if (kbps < acc->min_kbps) {
        acc->min_kbps = kbps;
    }
    if (kbps > acc->max_kbps) {
        acc->max_kbps = kbps;
    }
    acc->count++;
}

/* Mirrors dhara_error_t (dhara/dhara/dhara/error.h) values that indicate
 * FTL capacity/GC exhaustion rather than a genuine driver/hardware error.
 * Not included directly to avoid this example depending on Dhara's
 * internal headers; the ESP_ERR_FLASH_BASE + dhara_error_t encoding is a
 * stable convention used throughout spi_nand_flash/src/dhara_glue.c. */
#define EXAMPLE_DHARA_E_TOO_BAD       (ESP_ERR_FLASH_BASE + 3)  /* more bad blocks than expected / retries exhausted */
#define EXAMPLE_DHARA_E_JOURNAL_FULL  (ESP_ERR_FLASH_BASE + 5)  /* no free physical block to roll the journal onto */
#define EXAMPLE_DHARA_E_MAP_FULL      (ESP_ERR_FLASH_BASE + 7)  /* Dhara's live sector count reached its capacity */

/* Checks a write()/pwrite()/read()/pread()/fwrite()/fread() short result.
 * Returns true if this should be treated as a non-fatal FTL-capacity/GC
 * skip rather than a hard failure.
 *
 * FAT's logical free-space count and the NAND FTL's actual free *physical*
 * block count are different things: a write-amplification-heavy phase can
 * consume the wear-leveling reserve enough that a later write fails at the
 * Dhara layer even though FAT still reports free space. That failure is
 * *lossily* translated on the way up: dhara_glue.c returns
 * ESP_ERR_FLASH_BASE + dhara_error_t, diskio_nand.c collapses any non-OK
 * esp_err_t to RES_ERROR, FatFs' ff.c collapses any disk_write failure to
 * FR_DISK_ERR, and ESP-IDF's vfs_fat.c maps FR_DISK_ERR to errno=EIO. By
 * the time POSIX write() returns, the original Dhara-specific cause is
 * gone - EIO here could equally be an FTL-capacity condition or a genuine
 * driver/hardware error, and errno alone cannot tell them apart.
 *
 * To disambiguate, call spi_nand_flash_gc() on EIO: it goes through the
 * same dhara_glue.c translation, so its return code directly tells us
 * whether Dhara itself is reporting an FTL-capacity condition
 * (EXAMPLE_DHARA_E_MAP_FULL/_JOURNAL_FULL/_TOO_BAD) rather than guessing
 * from the lossy errno. ENOSPC (a different, non-lossy VFS FatFs path -
 * see vfs_fat.c's f_write()==FR_OK && written==0 case) is always treated
 * as FTL-capacity pressure without needing this extra check.
 */
static bool example_io_short_is_enospc_skip(spi_nand_flash_device_t *flash, const char *what,
                                            size_t expected, ssize_t n)
{
    int err = (n < 0) ? errno : 0;

    if (err != ENOSPC && err != EIO) {
        ESP_LOGE(TAG, "%s short: got %d, expected %u, errno=%d (%s)",
                 what, (int)n, (unsigned)expected, err, err ? strerror(err) : "n/a");
        return false;
    }

    if (err == ENOSPC) {
        ESP_LOGW(TAG, "%s short: ENOSPC (FTL/GC pressure, not a code bug) - skipping this cell", what);
        return true;
    }

    /* err == EIO: ask Dhara directly instead of guessing. */
    esp_err_t gc_ret = spi_nand_flash_gc(flash);
    bool is_ftl_pressure = (gc_ret == EXAMPLE_DHARA_E_MAP_FULL) ||
                           (gc_ret == EXAMPLE_DHARA_E_JOURNAL_FULL) ||
                           (gc_ret == EXAMPLE_DHARA_E_TOO_BAD);

    if (is_ftl_pressure) {
        ESP_LOGW(TAG, "%s short: errno=EIO, spi_nand_flash_gc()=0x%X confirms FTL/GC capacity "
                 "pressure (not a code bug) - skipping this cell", what, (unsigned)gc_ret);
        return true;
    }

    ESP_LOGE(TAG, "%s short: got %d, expected %u, errno=EIO, spi_nand_flash_gc()=0x%X "
             "(not a recognized FTL-capacity code - likely a real driver/hardware error)",
             what, (int)n, (unsigned)expected, (unsigned)gc_ret);
    return false;
}

/* On a short write/read: goto fail with ret=ESP_OK and skipped=true if it's
 * an FTL-capacity skip (run_chunk_matrix continues to the next cell), or
 * ret=ESP_FAIL otherwise. Requires `flash` and `skipped` in scope. */
#define EXAMPLE_CHECK_IO_OR_SKIP(n, expected, what)                             \
    do {                                                                       \
        if ((n) != (ssize_t)(expected)) {                                     \
            if (example_io_short_is_enospc_skip(flash, (what), (expected), (n))) { \
                skipped = true;                                                \
                ret = ESP_OK;                                                  \
            } else {                                                           \
                ret = ESP_FAIL;                                                \
            }                                                                  \
            goto fail;                                                        \
        }                                                                     \
    } while (0)

static double timing_acc_avg_kbps(const timing_acc_t *acc)
{
    return (acc->count > 0) ? (acc->sum_kbps / acc->count) : 0.0;
}

/* Start each measured pass with zeroed counters and a cold metadata cache. */
static esp_err_t begin_measured_pass(spi_nand_flash_device_t *flash)
{
    ESP_RETURN_ON_ERROR(nand_reset_perf_stats(flash), TAG, "reset perf stats failed");
    return nand_invalidate_metadata_cache(flash);
}

static double write_amplification(const spi_nand_flash_perf_stats_t *stats, size_t bytes_transferred)
{
    uint64_t physical_bytes = (stats->physical_programs + stats->physical_copies) * (uint64_t)s_page_size;
    return (s_page_size != 0 && bytes_transferred != 0) ?
           (double)physical_bytes / (double)bytes_transferred : 0.0;
}

static double read_amplification(const spi_nand_flash_perf_stats_t *stats, size_t bytes_transferred)
{
    uint64_t physical_bytes = stats->physical_reads * (uint64_t)s_page_size;
    return (s_page_size != 0 && bytes_transferred != 0) ?
           (double)physical_bytes / (double)bytes_transferred : 0.0;
}

typedef struct {
    bool skipped;
    const timing_acc_t *write_acc;
    const timing_acc_t *read_acc;
    const spi_nand_flash_perf_stats_t *write_stats;
    const spi_nand_flash_perf_stats_t *read_stats;
    size_t bytes_written;
    size_t bytes_read;
} example_result_data_t;

static void log_json_line(const char *tag_prefix, cJSON *obj)
{
    char *s = cJSON_PrintUnformatted(obj);
    if (s != NULL) {
        ESP_LOGI(TAG, "%s %s", tag_prefix, s);
        cJSON_free(s);
    }
    cJSON_Delete(obj);
}

static void log_result_json(const char *phase, size_t chunk_size, int tries,
                            const example_result_data_t *data)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "phase", phase);
    cJSON_AddNumberToObject(obj, "chunk", (double)chunk_size);
    cJSON_AddBoolToObject(obj, "skipped", data->skipped);
    if (!data->skipped) {
        cJSON_AddNumberToObject(obj, "tries", tries);
        cJSON_AddNumberToObject(obj, "write_avg_kbps", timing_acc_avg_kbps(data->write_acc));
        cJSON_AddNumberToObject(obj, "write_min_kbps", data->write_acc->min_kbps);
        cJSON_AddNumberToObject(obj, "write_max_kbps", data->write_acc->max_kbps);
        cJSON_AddNumberToObject(obj, "read_avg_kbps", timing_acc_avg_kbps(data->read_acc));
        cJSON_AddNumberToObject(obj, "read_min_kbps", data->read_acc->min_kbps);
        cJSON_AddNumberToObject(obj, "read_max_kbps", data->read_acc->max_kbps);
        cJSON_AddNumberToObject(obj, "write_wa", write_amplification(data->write_stats, data->bytes_written));
        cJSON_AddNumberToObject(obj, "read_wa", read_amplification(data->read_stats, data->bytes_read));
        /* Raw counters, summed over all tries of this cell. Read passes never
         * program, copy or erase, so only their reads and cache counts are kept. */
        const spi_nand_flash_perf_stats_t *w = data->write_stats;
        const spi_nand_flash_perf_stats_t *r = data->read_stats;
        cJSON_AddNumberToObject(obj, "w_reads", (double)w->physical_reads);
        cJSON_AddNumberToObject(obj, "w_programs", (double)w->physical_programs);
        cJSON_AddNumberToObject(obj, "w_copies", (double)w->physical_copies);
        cJSON_AddNumberToObject(obj, "w_erases", (double)w->physical_erases);
        cJSON_AddNumberToObject(obj, "w_meta_hits", (double)w->metadata_cache_hits);
        cJSON_AddNumberToObject(obj, "w_meta_misses", (double)w->metadata_cache_misses);
        cJSON_AddNumberToObject(obj, "r_reads", (double)r->physical_reads);
        cJSON_AddNumberToObject(obj, "r_meta_hits", (double)r->metadata_cache_hits);
        cJSON_AddNumberToObject(obj, "r_meta_misses", (double)r->metadata_cache_misses);
    }
    log_json_line("RESULT", obj);
}

static void log_throughput_stats(const char *label, const char *op, size_t chunk_size, const timing_acc_t *acc)
{
    double avg_kbps = timing_acc_avg_kbps(acc);
    ESP_LOGI(TAG, "[%s] chunk=%u %s: avg %.2f kB/s (min %.2f, max %.2f, %d run%s)",
             label, (unsigned)chunk_size, op, avg_kbps, acc->min_kbps, acc->max_kbps,
             acc->count, acc->count == 1 ? "" : "s");
}

/* Prints raw physical op counters plus a derived amplification ratio:
 *   write: (physical_programs + physical_copies) * page_size / bytes_transferred
 *   read:  physical_reads * page_size / bytes_transferred
 * physical_copies is included in write amplification because those are
 * GC-driven physical page writes attributable to the workload, same as SNIA
 * SSS PTS / littlefs-benchmarks treat GC-relocated writes as part of WA.
 */
static void log_perf_stats(const char *operation, const spi_nand_flash_perf_stats_t *stats, size_t bytes_transferred)
{
    uint64_t cache_accesses = stats->metadata_cache_hits + stats->metadata_cache_misses;
    double hit_rate = cache_accesses ?
                      (double)stats->metadata_cache_hits * 100.0 / (double)cache_accesses : 0.0;

    ESP_LOGI(TAG, "%s stats: reads=%" PRIu64 ", programs=%" PRIu64
             ", copies=%" PRIu64 ", erases=%" PRIu64
             ", metadata hits=%" PRIu64 ", misses=%" PRIu64 " (%.1f%% hit)",
             operation, stats->physical_reads, stats->physical_programs,
             stats->physical_copies, stats->physical_erases,
             stats->metadata_cache_hits, stats->metadata_cache_misses, hit_rate);

    if (s_page_size == 0 || bytes_transferred == 0) {
        return;
    }

    if (strcmp(operation, "write") == 0) {
        uint64_t physical_bytes = (stats->physical_programs + stats->physical_copies) * (uint64_t)s_page_size;
        double wa = write_amplification(stats, bytes_transferred);
        ESP_LOGI(TAG, "%s amplification: %.2fx (%" PRIu64 " physical B / %u logical B)",
                 operation, wa, physical_bytes, (unsigned)bytes_transferred);
    } else {
        uint64_t physical_bytes = stats->physical_reads * (uint64_t)s_page_size;
        double ra = read_amplification(stats, bytes_transferred);
        ESP_LOGI(TAG, "%s amplification: %.2fx (%" PRIu64 " physical B / %u logical B)",
                 operation, ra, physical_bytes, (unsigned)bytes_transferred);
    }
}

/*
 * Phase 1 - stdio, default (block) buffering. This is the realistic "naive
 * fopen/fwrite/fread" cost: unlike an earlier revision of this example, we do
 * NOT force _IONBF here, because an unbuffered stdio stream degenerates into
 * the same write()/read() syscall pattern as the POSIX phase below and stops
 * measuring anything the POSIX phase doesn't already cover. Leaving the
 * default buffering in place is what actually differs: stdio batches small
 * writes into its internal buffer before issuing a syscall.
 */
static esp_err_t fatfs_throughput_stdio(spi_nand_flash_device_t *flash, size_t chunk_size)
{
    esp_err_t ret = ESP_OK;
    bool skipped = false;
    uint8_t *buf = NULL;
    FILE *f = NULL;
    spi_nand_flash_perf_stats_t write_stats = {0};
    spi_nand_flash_perf_stats_t read_stats = {0};
    const int tries = CONFIG_EXAMPLE_TEST_TRIES;
    timing_acc_t write_acc, read_acc;
    timing_acc_init(&write_acc);
    timing_acc_init(&read_acc);

    ESP_RETURN_ON_FALSE(EXAMPLE_TEST_FILE_SIZE % chunk_size == 0, ESP_ERR_INVALID_ARG, TAG,
                        "file size must be a multiple of chunk size");

    buf = (uint8_t *)heap_caps_aligned_alloc(64, chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    memset(buf, 0xA5, chunk_size);

    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    for (int t = 0; t < tries; t++) {
        unlink(EXAMPLE_TEST_FILE_PATH);
        int64_t start = esp_timer_get_time();
        f = fopen(EXAMPLE_TEST_FILE_PATH, "wb");
        ESP_GOTO_ON_FALSE(f != NULL, ESP_FAIL, fail, TAG, "fopen(wb) failed");

        for (size_t done = 0; done < EXAMPLE_TEST_FILE_SIZE; done += chunk_size) {
            size_t n = fwrite(buf, 1, chunk_size, f);
            EXAMPLE_CHECK_IO_OR_SKIP((ssize_t)n, chunk_size, "fwrite");
        }
        ESP_GOTO_ON_FALSE(fclose(f) == 0, ESP_FAIL, fail, TAG, "fclose(write) failed");
        f = NULL;
        timing_acc_add(&write_acc, EXAMPLE_TEST_FILE_SIZE, esp_timer_get_time() - start);
    }
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &write_stats), fail, TAG, "");

    memset(buf, 0x00, chunk_size);
    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    for (int t = 0; t < tries; t++) {
        int64_t start = esp_timer_get_time();
        f = fopen(EXAMPLE_TEST_FILE_PATH, "rb");
        ESP_GOTO_ON_FALSE(f != NULL, ESP_FAIL, fail, TAG, "fopen(rb) failed");

        for (size_t done = 0; done < EXAMPLE_TEST_FILE_SIZE; done += chunk_size) {
            size_t n = fread(buf, 1, chunk_size, f);
            EXAMPLE_CHECK_IO_OR_SKIP((ssize_t)n, chunk_size, "fread");
        }
        ESP_GOTO_ON_FALSE(fclose(f) == 0, ESP_FAIL, fail, TAG, "fclose(read) failed");
        f = NULL;
        timing_acc_add(&read_acc, EXAMPLE_TEST_FILE_SIZE, esp_timer_get_time() - start);
    }
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &read_stats), fail, TAG, "");

fail:
    /* Only clean success has complete timing accumulators and perf snapshots.
     * A graceful skip emits a flagged RESULT row without reading partial data;
     * a real failure emits no RESULT row and is reported by the caller. */
    if (ret == ESP_OK && !skipped) {
        const size_t bytes_transferred = (size_t)EXAMPLE_TEST_FILE_SIZE * tries;
        log_throughput_stats("stdio/buffered", "write", chunk_size, &write_acc);
        log_throughput_stats("stdio/buffered", "read", chunk_size, &read_acc);
        log_perf_stats("write", &write_stats, bytes_transferred);
        log_perf_stats("read", &read_stats, bytes_transferred);
        log_result_json("stdio/buffered", chunk_size, tries, &(example_result_data_t) {
            .skipped = false,
            .write_acc = &write_acc,
            .read_acc = &read_acc,
            .write_stats = &write_stats,
            .read_stats = &read_stats,
            .bytes_written = bytes_transferred,
            .bytes_read = bytes_transferred,
        });
    } else if (ret == ESP_OK && skipped) {
        log_result_json("stdio/buffered", chunk_size, tries,
        &(example_result_data_t) {
            .skipped = true
        });
    }
    /* Reached on success (fall-through above) and on error/skip (goto) alike,
     * so this unconditional unlink is the single cleanup point for both -
     * a failed/skipped cell must not leak its test file and starve free
     * space for later rounds/phases. Close before unlink: FatFs is not
     * guaranteed to handle unlinking a still-open file cleanly. */
    if (f != NULL) {
        fclose(f);
    }
    unlink(EXAMPLE_TEST_FILE_PATH);
    free(buf);
    return ret;
}

/*
 * Phase 2 - POSIX read/write, sequential (no stdio layer). Prefer chunk ==
 * cluster size. close() is included in the timed path so FatFs/Dhara flush
 * is counted.
 */
static esp_err_t fatfs_throughput_posix(spi_nand_flash_device_t *flash, size_t chunk_size)
{
    esp_err_t ret = ESP_OK;
    bool skipped = false;
    uint8_t *buf = NULL;
    int fd = -1;
    spi_nand_flash_perf_stats_t write_stats = {0};
    spi_nand_flash_perf_stats_t read_stats = {0};
    const int tries = CONFIG_EXAMPLE_TEST_TRIES;
    timing_acc_t write_acc, read_acc;
    timing_acc_init(&write_acc);
    timing_acc_init(&read_acc);

    ESP_RETURN_ON_FALSE(EXAMPLE_TEST_FILE_SIZE % chunk_size == 0, ESP_ERR_INVALID_ARG, TAG,
                        "file size must be a multiple of chunk size");

    buf = (uint8_t *)heap_caps_aligned_alloc(64, chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    memset(buf, 0xA5, chunk_size);

    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    for (int t = 0; t < tries; t++) {
        unlink(EXAMPLE_TEST_FILE_PATH);
        int64_t start = esp_timer_get_time();
        fd = open(EXAMPLE_TEST_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "open(O_WRONLY) failed: %d", errno);

        for (size_t done = 0; done < EXAMPLE_TEST_FILE_SIZE; done += chunk_size) {
            ssize_t n = write(fd, buf, chunk_size);
            EXAMPLE_CHECK_IO_OR_SKIP(n, chunk_size, "write");
        }
        ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "close(write) failed");
        fd = -1;
        timing_acc_add(&write_acc, EXAMPLE_TEST_FILE_SIZE, esp_timer_get_time() - start);
    }
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &write_stats), fail, TAG, "");

    memset(buf, 0x00, chunk_size);
    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    for (int t = 0; t < tries; t++) {
        int64_t start = esp_timer_get_time();
        fd = open(EXAMPLE_TEST_FILE_PATH, O_RDONLY);
        ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "open(O_RDONLY) failed: %d", errno);

        for (size_t done = 0; done < EXAMPLE_TEST_FILE_SIZE; done += chunk_size) {
            ssize_t n = read(fd, buf, chunk_size);
            EXAMPLE_CHECK_IO_OR_SKIP(n, chunk_size, "read");
        }
        ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "close(read) failed");
        fd = -1;
        timing_acc_add(&read_acc, EXAMPLE_TEST_FILE_SIZE, esp_timer_get_time() - start);
    }
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &read_stats), fail, TAG, "");

fail:
    /* Keep the success/skip/error cases separate so a skip never reads its
     * partially-filled timing accumulators or unsnapshotted perf counters. */
    if (ret == ESP_OK && !skipped) {
        const size_t bytes_transferred = (size_t)EXAMPLE_TEST_FILE_SIZE * tries;
        log_throughput_stats("posix sequential", "write", chunk_size, &write_acc);
        log_throughput_stats("posix sequential", "read", chunk_size, &read_acc);
        log_perf_stats("write", &write_stats, bytes_transferred);
        log_perf_stats("read", &read_stats, bytes_transferred);
        log_result_json("posix sequential", chunk_size, tries, &(example_result_data_t) {
            .skipped = false,
            .write_acc = &write_acc,
            .read_acc = &read_acc,
            .write_stats = &write_stats,
            .read_stats = &read_stats,
            .bytes_written = bytes_transferred,
            .bytes_read = bytes_transferred,
        });
    } else if (ret == ESP_OK && skipped) {
        log_result_json("posix sequential", chunk_size, tries,
        &(example_result_data_t) {
            .skipped = true
        });
    }
    /* Reached on success and error/skip alike - see fatfs_throughput_stdio's
     * fail: comment for why this unlink must be unconditional. */
    if (fd >= 0) {
        close(fd);
    }
    unlink(EXAMPLE_TEST_FILE_PATH);
    free(buf);
    return ret;
}

/*
 * Phase 3 - POSIX pwrite/pread, chunks visited in randomized order within
 * the same file/size envelope as phase 2. This is the access-pattern axis
 * that a purely-sequential benchmark misses: random-order writes force
 * FAT/Dhara to build the cluster/page mapping out of order instead of
 * simply extending it, which is far closer to real-world logging/DB-style
 * workloads than one giant sequential file.
 */
static esp_err_t fatfs_throughput_posix_random(spi_nand_flash_device_t *flash, size_t chunk_size)
{
    esp_err_t ret = ESP_OK;
    bool skipped = false;
    uint8_t *buf = NULL;
    size_t *order = NULL;
    int fd = -1;
    spi_nand_flash_perf_stats_t write_stats = {0};
    spi_nand_flash_perf_stats_t read_stats = {0};
    const int tries = CONFIG_EXAMPLE_TEST_TRIES;
    timing_acc_t write_acc, read_acc;
    timing_acc_init(&write_acc);
    timing_acc_init(&read_acc);

    ESP_RETURN_ON_FALSE(EXAMPLE_TEST_FILE_SIZE % chunk_size == 0, ESP_ERR_INVALID_ARG, TAG,
                        "file size must be a multiple of chunk size");

    size_t chunk_count = EXAMPLE_TEST_FILE_SIZE / chunk_size;
    buf = (uint8_t *)heap_caps_aligned_alloc(64, chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    order = (size_t *)malloc(chunk_count * sizeof(size_t));
    ESP_GOTO_ON_FALSE(order != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");
    memset(buf, 0xA5, chunk_size);

    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    for (int t = 0; t < tries; t++) {
        unlink(EXAMPLE_TEST_FILE_PATH);
        fd = open(EXAMPLE_TEST_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "open(O_WRONLY) failed: %d", errno);
        for (size_t i = 0; i < chunk_count; i++) {
            order[i] = i;
        }
        shuffle_indices(order, chunk_count);

        int64_t start = esp_timer_get_time();
        for (size_t i = 0; i < chunk_count; i++) {
            off_t offset = (off_t)(order[i] * chunk_size);
            ssize_t n = pwrite(fd, buf, chunk_size, offset);
            EXAMPLE_CHECK_IO_OR_SKIP(n, chunk_size, "pwrite");
        }
        int64_t elapsed = esp_timer_get_time() - start;
        ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "close(write) failed");
        fd = -1;
        timing_acc_add(&write_acc, EXAMPLE_TEST_FILE_SIZE, elapsed);
    }
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &write_stats), fail, TAG, "");

    memset(buf, 0x00, chunk_size);
    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    for (int t = 0; t < tries; t++) {
        fd = open(EXAMPLE_TEST_FILE_PATH, O_RDONLY);
        ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "open(O_RDONLY) failed: %d", errno);
        for (size_t i = 0; i < chunk_count; i++) {
            order[i] = i;
        }
        shuffle_indices(order, chunk_count);

        int64_t start = esp_timer_get_time();
        for (size_t i = 0; i < chunk_count; i++) {
            off_t offset = (off_t)(order[i] * chunk_size);
            ssize_t n = pread(fd, buf, chunk_size, offset);
            EXAMPLE_CHECK_IO_OR_SKIP(n, chunk_size, "pread");
        }
        int64_t elapsed = esp_timer_get_time() - start;
        ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "close(read) failed");
        fd = -1;
        timing_acc_add(&read_acc, EXAMPLE_TEST_FILE_SIZE, elapsed);
    }
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &read_stats), fail, TAG, "");

fail:
    /* Keep the success/skip/error cases separate so a skip never reads its
     * partially-filled timing accumulators or unsnapshotted perf counters. */
    if (ret == ESP_OK && !skipped) {
        const size_t bytes_transferred = (size_t)EXAMPLE_TEST_FILE_SIZE * tries;
        log_throughput_stats("posix random", "write", chunk_size, &write_acc);
        log_throughput_stats("posix random", "read", chunk_size, &read_acc);
        log_perf_stats("write", &write_stats, bytes_transferred);
        log_perf_stats("read", &read_stats, bytes_transferred);
        log_result_json("posix random", chunk_size, tries, &(example_result_data_t) {
            .skipped = false,
            .write_acc = &write_acc,
            .read_acc = &read_acc,
            .write_stats = &write_stats,
            .read_stats = &read_stats,
            .bytes_written = bytes_transferred,
            .bytes_read = bytes_transferred,
        });
    } else if (ret == ESP_OK && skipped) {
        log_result_json("posix random", chunk_size, tries,
        &(example_result_data_t) {
            .skipped = true
        });
    }
    /* Reached on success and error/skip alike - see fatfs_throughput_stdio's
     * fail: comment for why this unlink must be unconditional. */
    if (fd >= 0) {
        close(fd);
    }
    unlink(EXAMPLE_TEST_FILE_PATH);
    free(order);
    free(buf);
    return ret;
}

/*
 * Phase 4 - POSIX sequential read/write, but split into EXAMPLE_MULTIFILE_COUNT
 * files instead of one. Isolates directory-entry/cluster-chain overhead and
 * fragmentation cost that a single-file test can never see, at a total byte
 * count derived from actual free space (see s_multifile_per_file_bytes).
 */
static esp_err_t fatfs_throughput_multifile(spi_nand_flash_device_t *flash, size_t chunk_size, size_t per_file_bytes)
{
    esp_err_t ret = ESP_OK;
    bool skipped = false;
    uint8_t *buf = NULL;
    int fd = -1;
    char path[64];
    spi_nand_flash_perf_stats_t write_stats = {0};
    spi_nand_flash_perf_stats_t read_stats = {0};
    const int tries = 1;
    timing_acc_t write_acc, read_acc;
    timing_acc_init(&write_acc);
    timing_acc_init(&read_acc);

    ESP_RETURN_ON_FALSE(per_file_bytes % chunk_size == 0, ESP_ERR_INVALID_ARG, TAG,
                        "per-file size must be a multiple of chunk size");

    buf = (uint8_t *)heap_caps_aligned_alloc(64, chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    memset(buf, 0xA5, chunk_size);

    for (int i = 0; i < EXAMPLE_MULTIFILE_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/mf_%d.bin", base_path, i);
        unlink(path);
    }

    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    int64_t start = esp_timer_get_time();
    for (int i = 0; i < EXAMPLE_MULTIFILE_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/mf_%d.bin", base_path, i);
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "open(O_WRONLY) failed: %d", errno);
        for (size_t done = 0; done < per_file_bytes; done += chunk_size) {
            ssize_t n = write(fd, buf, chunk_size);
            EXAMPLE_CHECK_IO_OR_SKIP(n, chunk_size, "write");
        }
        ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "close(write) failed");
        fd = -1;
    }
    timing_acc_add(&write_acc, per_file_bytes * EXAMPLE_MULTIFILE_COUNT, esp_timer_get_time() - start);
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &write_stats), fail, TAG, "");

    memset(buf, 0x00, chunk_size);
    ESP_GOTO_ON_ERROR(begin_measured_pass(flash), fail, TAG, "");
    start = esp_timer_get_time();
    for (int i = 0; i < EXAMPLE_MULTIFILE_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/mf_%d.bin", base_path, i);
        fd = open(path, O_RDONLY);
        ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "open(O_RDONLY) failed: %d", errno);
        for (size_t done = 0; done < per_file_bytes; done += chunk_size) {
            ssize_t n = read(fd, buf, chunk_size);
            EXAMPLE_CHECK_IO_OR_SKIP(n, chunk_size, "read");
        }
        ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "close(read) failed");
        fd = -1;
    }
    timing_acc_add(&read_acc, per_file_bytes * EXAMPLE_MULTIFILE_COUNT, esp_timer_get_time() - start);
    ESP_GOTO_ON_ERROR(nand_get_perf_stats(flash, &read_stats), fail, TAG, "");

fail:
    /* Keep the success/skip/error cases separate so a skip never reads its
     * partially-filled timing accumulators or unsnapshotted perf counters. */
    if (ret == ESP_OK && !skipped) {
        const size_t bytes_transferred = per_file_bytes * EXAMPLE_MULTIFILE_COUNT;
        log_throughput_stats("posix multi-file", "write", chunk_size, &write_acc);
        log_throughput_stats("posix multi-file", "read", chunk_size, &read_acc);
        log_perf_stats("write", &write_stats, bytes_transferred);
        log_perf_stats("read", &read_stats, bytes_transferred);
        log_result_json("posix multi-file", chunk_size, tries, &(example_result_data_t) {
            .skipped = false,
            .write_acc = &write_acc,
            .read_acc = &read_acc,
            .write_stats = &write_stats,
            .read_stats = &read_stats,
            .bytes_written = bytes_transferred,
            .bytes_read = bytes_transferred,
        });
    } else if (ret == ESP_OK && skipped) {
        log_result_json("posix multi-file", chunk_size, tries,
        &(example_result_data_t) {
            .skipped = true
        });
    }
    /* Reached on success and error/skip alike - see fatfs_throughput_stdio's
     * fail: comment for why this cleanup must be unconditional. A failed
     * write loop may have created anywhere from 0 to EXAMPLE_MULTIFILE_COUNT
     * files; unlink() on a nonexistent path is a harmless no-op, so it's
     * simplest to just sweep all of them every time. */
    if (fd >= 0) {
        close(fd);
    }
    for (int i = 0; i < EXAMPLE_MULTIFILE_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/mf_%d.bin", base_path, i);
        unlink(path);
    }
    free(buf);
    return ret;
}

/* Adapter so the multi-file phase fits run_chunk_matrix()'s function-pointer
 * signature; per-file byte target is computed once in app_main from actual
 * free space and stashed in s_multifile_per_file_bytes. */
static esp_err_t fatfs_throughput_multifile_phase(spi_nand_flash_device_t *flash, size_t chunk_size)
{
    size_t per_file_bytes = (s_multifile_per_file_bytes / chunk_size) * chunk_size;
    if (per_file_bytes == 0) {
        ESP_LOGW(TAG, "multi-file: per-file budget too small for chunk=%u, skipping",
                 (unsigned)chunk_size);
        return ESP_OK;
    }
    return fatfs_throughput_multifile(flash, chunk_size, per_file_bytes);
}

/* Optional preconditioning (CONFIG_EXAMPLE_PRECONDITION_ENABLE): writes a
 * target_bytes fill file and keeps it, so the FTL holds that much live data
 * during the timed phases. Deleting it would not work: FatFs trims freed
 * clusters (FF_USE_TRIM), which leaves the FTL as empty as a fresh chip.
 * Any write failure aborts the run; the file is removed only on failure.
 *
 * Guarded by the same #if as its only call site (in app_main): when the
 * option is off, this must not be compiled in at all, or -Werror flags it
 * as an unused static function. */
#if CONFIG_EXAMPLE_PRECONDITION_ENABLE
static esp_err_t example_precondition_fs(size_t target_bytes)
{
    if (target_bytes == 0) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    uint8_t *buf = NULL;
    int fd = -1;
    const size_t chunk = EXAMPLE_ALLOC_UNIT_SIZE;
    const char *path = "/nandflash/aging_fill.bin";

    buf = (uint8_t *)heap_caps_aligned_alloc(64, chunk, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    memset(buf, 0x5A, chunk);

    ESP_LOGI(TAG, "Preconditioning: writing %u kB fill file, kept during the timed phases",
             (unsigned)(target_bytes / 1024));

    int64_t start = esp_timer_get_time();
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, fail, TAG, "precondition open failed: %d", errno);

    for (size_t done = 0; done < target_bytes; done += chunk) {
        size_t this_chunk = (target_bytes - done < chunk) ? (target_bytes - done) : chunk;
        ssize_t n = write(fd, buf, this_chunk);
        ESP_GOTO_ON_FALSE(n == (ssize_t)this_chunk, ESP_FAIL, fail, TAG,
                          "precondition write failed at %u kB: n=%d errno=%d",
                          (unsigned)(done / 1024), (int)n, errno);
    }
    ESP_GOTO_ON_FALSE(close(fd) == 0, ESP_FAIL, fail, TAG, "precondition close failed");
    fd = -1;
    ESP_LOGI(TAG, "Preconditioning done in %" PRId64 " ms", (esp_timer_get_time() - start) / 1000);
    free(buf);
    return ESP_OK;

fail:
    if (fd >= 0) {
        close(fd);
    }
    unlink(path);
    free(buf);
    return ret;
}
#endif /* CONFIG_EXAMPLE_PRECONDITION_ENABLE */

static esp_err_t run_chunk_matrix(spi_nand_flash_device_t *flash,
                                  esp_err_t (*fn)(spi_nand_flash_device_t *, size_t),
                                  const char *phase)
{
    ESP_LOGI(TAG, "======== %s ========", phase);
    for (size_t i = 0; i < sizeof(s_chunk_sizes) / sizeof(s_chunk_sizes[0]); i++) {
        /* Skip duplicate if 16 KiB == cluster size. */
        if (i > 0 && s_chunk_sizes[i] == s_chunk_sizes[i - 1]) {
            continue;
        }
        esp_err_t ret = fn(flash, s_chunk_sizes[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "%s failed for chunk %u: %s",
                     phase, (unsigned)s_chunk_sizes[i], esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    srand((unsigned int)esp_timer_get_time());

    spi_device_handle_t spi;
    spi_nand_flash_device_t *flash;
    example_init_nand_flash(&flash, &spi);
    if (flash == NULL) {
        return;
    }

    ESP_ERROR_CHECK(spi_nand_flash_get_page_size(flash, &s_page_size));

    ESP_LOGW(TAG, "Erasing the entire NAND so this destructive benchmark starts from a known state");
    ESP_ERROR_CHECK(spi_nand_erase_chip(flash));

    esp_vfs_fat_mount_config_t config = {
        .max_files = 4,
        /* true so EXAMPLE_ALLOC_UNIT_SIZE applies on first bring-up / failed mount. */
        .format_if_mount_failed = true,
        .allocation_unit_size = EXAMPLE_ALLOC_UNIT_SIZE,
    };

    esp_err_t ret = esp_vfs_fat_nand_mount(base_path, flash, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_nand_mount failed: %s", esp_err_to_name(ret));
        example_deinit_nand_flash(flash, spi);
        return;
    }

    uint64_t bytes_total, bytes_free;
    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free",
             bytes_total / 1024, bytes_free / 1024);
    ESP_LOGI(TAG, "SPI: %d kHz, %s; cluster=%d; test file=%d bytes; tries=%d",
             EXAMPLE_FLASH_FREQ_KHZ, io_mode_str(EXAMPLE_IO_MODE),
             EXAMPLE_ALLOC_UNIT_SIZE, EXAMPLE_TEST_FILE_SIZE, CONFIG_EXAMPLE_TEST_TRIES);
    ESP_LOGI(TAG, "Volume was erased before mount; format uses the configured allocation unit");

    /* Emit the run configuration only after mounting, when the actual volume
     * capacity is available. Keep it directly after the cache overview so
     * captured logs place the human and machine-readable config together. */
    print_cache_kconfig_overview();
    cJSON *run = cJSON_CreateObject();
#if CONFIG_DHARA_MAP_PATH_CACHE
    cJSON_AddBoolToObject(run, "path_cache", CONFIG_DHARA_MAP_PATH_CACHE);
#else
    cJSON_AddBoolToObject(run, "path_cache", false);
#endif
#ifdef CONFIG_NAND_FLASH_PAGE_REGISTER_CACHE
    cJSON_AddBoolToObject(run, "page_reg_cache", true);
#else
    cJSON_AddBoolToObject(run, "page_reg_cache", false);
#endif
#ifdef CONFIG_NAND_FLASH_DHARA_META_CACHE
    cJSON_AddBoolToObject(run, "meta_cache", true);
    cJSON_AddNumberToObject(run, "meta_slots", CONFIG_NAND_FLASH_DHARA_META_CACHE_SLOTS);
#else
    cJSON_AddBoolToObject(run, "meta_cache", false);
    cJSON_AddNumberToObject(run, "meta_slots", 0);
#endif
#if CONFIG_EXAMPLE_PRECONDITION_ENABLE
    cJSON_AddBoolToObject(run, "precondition", true);
    cJSON_AddNumberToObject(run, "precond_percent", CONFIG_EXAMPLE_PRECONDITION_FILL_PERCENT);
#else
    cJSON_AddBoolToObject(run, "precondition", false);
    cJSON_AddNumberToObject(run, "precond_percent", 0);
#endif
    cJSON_AddNumberToObject(run, "tries", CONFIG_EXAMPLE_TEST_TRIES);
    cJSON_AddNumberToObject(run, "chip_kb", (double)(bytes_total / 1024));
    cJSON_AddNumberToObject(run, "file_kb", EXAMPLE_TEST_FILE_SIZE / 1024);
    cJSON_AddNumberToObject(run, "cluster_kb", EXAMPLE_ALLOC_UNIT_SIZE / 1024);
    log_json_line("RUN", run);

#if CONFIG_EXAMPLE_PRECONDITION_ENABLE
    size_t precondition_bytes = (size_t)(bytes_free * CONFIG_EXAMPLE_PRECONDITION_FILL_PERCENT / 100);
    ret = example_precondition_fs(precondition_bytes);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "preconditioning failed: %s", esp_err_to_name(ret));
        esp_vfs_fat_nand_unmount(base_path, flash);
        example_deinit_nand_flash(flash, spi);
        return;
    }
    /* The fill file stays, so size the multi-file phase from what is left. */
    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
#endif

    size_t multifile_total_bytes = (size_t)(bytes_free * EXAMPLE_MULTIFILE_FREE_PERCENT / 100);
    s_multifile_per_file_bytes = multifile_total_bytes / EXAMPLE_MULTIFILE_COUNT;
    char multifile_phase_label[80];
    snprintf(multifile_phase_label, sizeof(multifile_phase_label),
             "posix: multi-file (%d files, ~%u kB each)", EXAMPLE_MULTIFILE_COUNT,
             (unsigned)(s_multifile_per_file_bytes / 1024));

    ret = run_chunk_matrix(flash, fatfs_throughput_stdio, "stdio: default buffering");
    if (ret == ESP_OK) {
        ret = run_chunk_matrix(flash, fatfs_throughput_posix, "posix: sequential read/write");
    }
    if (ret == ESP_OK) {
        ret = run_chunk_matrix(flash, fatfs_throughput_posix_random, "posix: random-order pwrite/pread");
    }
    if (ret == ESP_OK) {
        ret = run_chunk_matrix(flash, fatfs_throughput_multifile_phase, multifile_phase_label);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "throughput suite failed: %s", esp_err_to_name(ret));
    }

    esp_vfs_fat_nand_unmount(base_path, flash);
    example_deinit_nand_flash(flash, spi);
}
