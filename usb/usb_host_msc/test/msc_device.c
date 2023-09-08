/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


#include "esp_log.h"
#include "tinyusb.h"
#include "esp_idf_version.h"
#include "soc/soc_caps.h"
#include "test_common.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_check.h"
#include "driver/gpio.h"
#include "tusb_msc_storage.h"
#endif /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_SDMMC_HOST_SUPPORTED
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_SDMMC_HOST_SUPPORTED */

#if SOC_USB_OTG_SUPPORTED

/* sd-card configuration to be done by user */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_SDMMC_HOST_SUPPORTED
#define SDMMC_BUS_WIDTH 4 /* Select the bus width of SD or MMC interface (4 or 1).
                            Note that even if 1 line mode is used, D3 pin of the SD card must
                            have a pull-up resistor connected. Otherwise the card may enter
                            SPI mode, the only way to recover from which is to cycle power to the card. */
#define PIN_CMD         35 /* CMD GPIO number */
#define PIN_CLK         36 /* CLK GPIO number */
#define PIN_D0          37 /* D0 GPIO number */
#define PIN_D1          38 /* D1 GPIO number (applicable when width SDMMC_BUS_WIDTH is 4) */
#define PIN_D2          33 /* D2 GPIO number (applicable when width SDMMC_BUS_WIDTH is 4) */
#define PIN_D3          34 /* D3 GPIO number (applicable when width SDMMC_BUS_WIDTH is 4) */
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_SDMMC_HOST_SUPPORTED */

static const char *TAG = "msc_example";

/* TinyUSB descriptors
   ********************************************************************* */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    // We intentionally do not implement Serial String descriptor to make sure that the driver can handle it
    //"123456",                       // 3: Serials
    //"Test MSC",                  // 4. MSC
};
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) */
/*********************************************************************** TinyUSB descriptors*/

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define VBUS_MONITORING_GPIO_NUM GPIO_NUM_4
static void configure_vbus_monitoring(void)
{
    // Configure GPIO Pin for vbus monitoring
    const gpio_config_t vbus_gpio_config = {
        .pin_bit_mask = BIT64(VBUS_MONITORING_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&vbus_gpio_config));
}
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) */

static void storage_init(void)
{
    ESP_LOGI(TAG, "USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .device_descriptor = &descriptor_config,
        .configuration_descriptor = desc_configuration,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .self_powered = true,
        .vbus_monitor_io = VBUS_MONITORING_GPIO_NUM
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) */
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) */

void device_app(void)
{
    ESP_LOGI(TAG, "Initializing storage...");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    configure_vbus_monitoring();

    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    tinyusb_msc_spiflash_config_t config_spi;
    config_spi.wl_handle = wl_handle;
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) */
    storage_init();
    while (1) {
        vTaskDelay(100);
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_SDMMC_HOST_SUPPORTED
static esp_err_t storage_init_sdmmc(sdmmc_card_t **card)
{
    esp_err_t ret = ESP_OK;
    bool host_init = false;
    sdmmc_card_t *sd_card;

    ESP_LOGI(TAG, "Initializing SDCard");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    if (SDMMC_BUS_WIDTH == 4) {
        slot_config.width = 4;
    } else {
        slot_config.width = 1;
    }

    // On chips where the GPIOs used for SD card can be configured, set the user defined values
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = PIN_CLK;
    slot_config.cmd = PIN_CMD;
    slot_config.d0 = PIN_D0;
    if (SDMMC_BUS_WIDTH == 4) {
        slot_config.d1 = PIN_D1;
        slot_config.d2 = PIN_D2;
        slot_config.d3 = PIN_D3;
    }
#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // not using ff_memalloc here, as allocation in internal RAM is preferred
    sd_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    ESP_GOTO_ON_FALSE(sd_card, ESP_ERR_NO_MEM, clean, TAG, "could not allocate new sdmmc_card_t");

    ESP_GOTO_ON_ERROR((*host.init)(), clean, TAG, "Host Config Init fail");
    host_init = true;

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(host.slot, (const sdmmc_slot_config_t *) &slot_config),
                      clean, TAG, "Host init slot fail");

    ESP_GOTO_ON_ERROR(sdmmc_card_init(&host, sd_card),
                      clean, TAG, "The detection pin of the slot is disconnected");

    *card = sd_card;

    return ESP_OK;

clean:
    if (host_init) {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
            host.deinit_p(host.slot);
        } else {
            (*host.deinit)();
        }
    }
    if (sd_card) {
        free(sd_card);
        sd_card = NULL;
    }
    return ret;
}

void device_app_sdmmc(void)
{
    ESP_LOGI(TAG, "Initializing storage...");
    configure_vbus_monitoring();
    static sdmmc_card_t *card = NULL;
    ESP_ERROR_CHECK(storage_init_sdmmc(&card));

    tinyusb_msc_sdmmc_config_t config_sdmmc;
    config_sdmmc.card = card;
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));

    storage_init();
    while (1) {
        vTaskDelay(100);
    }
}
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_SDMMC_HOST_SUPPORTED */

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
// whether host does safe-eject
static bool ejected = false;

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined

uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] = {
    //------------- Block0: Boot Sector -------------//
    // byte_per_sector    = DISK_BLOCK_SIZE; fat12_sector_num_16  = DISK_BLOCK_NUM;
    // sector_per_cluster = 1; reserved_sectors = 1;
    // fat_num            = 1; fat12_root_entry_num = 16;
    // sector_per_fat     = 1; sector_per_track = 1; head_num = 1; hidden_sectors = 0;
    // drive_number       = 0x80; media_type = 0xf8; extended_boot_signature = 0x29;
    // filesystem_type    = "FAT12   "; volume_serial_number = 0x1234; volume_label = "TinyUSB MSC";
    // FAT magic code at offset 510-511
    {
        0xEB, 0x3C, 0x90, 0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00,
        0x01, 0x10, 0x00, 0x10, 0x00, 0xF8, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00, 'T', 'i', 'n', 'y', 'U',
        'S', 'B', ' ', 'M', 'S', 'C', 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00,

        // Zero up to 2 last bytes of FAT magic code
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 'F', 'A', 'T', '3', '2', ' ', ' ', ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA
    },

    //------------- Block1: FAT12 Table -------------//
    {
        0xF8, 0xFF, 0xFF, 0xFF, 0x0F // // first 2 entries must be F8FF, third entry is cluster end of readme file
    },

    //------------- Block2: Root Directory -------------//
    {
        // first entry is volume label
        'T', 'i', 'n', 'y', 'U', 'S', 'B', ' ', 'M', 'S', 'C', 0x08, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // second entry is readme file
        'R', 'E', 'A', 'D', 'M', 'E', ' ', ' ', 'T', 'X', 'T', 0x20, 0x00, 0xC6, 0x52, 0x6D,
        0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
        sizeof(README_CONTENTS) - 1, 0x00, 0x00, 0x00 // readme's files size (4 Bytes)
    },

    //------------- Block3: Readme Content -------------//
    README_CONTENTS
};

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void) lun;

    const char vid[] = "TinyUSB";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void) lun;

    // RAM disk is ready until ejected
    if (ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void) lun;

    *block_count = DISK_BLOCK_NUM;
    *block_size  = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void) lun;
    (void) power_condition;

    if ( load_eject ) {
        if (start) {
            // load disk storage
        } else {
            // unload disk storage
            ejected = true;
        }
    }

    return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void) lun;

    uint8_t const *addr = msc_disk[lba] + offset;
    memcpy(buffer, addr, bufsize);

    return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    (void) lun;

#ifndef CFG_EXAMPLE_MSC_READONLY
    uint8_t *addr = msc_disk[lba] + offset;
    memcpy(addr, buffer, bufsize);
#else
    (void) lba; (void) offset; (void) buffer;
#endif

    return bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    // read10 & write10 has their own callback and MUST not be handled here

    void const *response = NULL;
    uint16_t resplen = 0;

    // most scsi handled is input
    bool in_xfer = true;

    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        // Host is about to read/write etc ... better not to disconnect disk
        resplen = 0;
        break;

    default:
        // Set Sense = Invalid Command Operation
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

        // negative means error -> tinyusb could stall and/or response with failed status
        resplen = -1;
        break;
    }

    // return resplen must not larger than bufsize
    if ( resplen > bufsize ) {
        resplen = bufsize;
    }

    if ( response && (resplen > 0) ) {
        if (in_xfer) {
            memcpy(buffer, response, resplen);
        } else {
            // SCSI output
        }
    }

    return resplen;
}
#endif /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) */

#endif /* SOC_USB_OTG_SUPPORTED */
