/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nand_device_types.h"
#include "spi_nand_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief nand_ecc_max_bits_corrected() result when the status carries no correction count. */
#define NAND_ECC_BITS_UNKNOWN UINT8_MAX

/** @brief Extract the 2-bit ECC status field (bits [5:4]) used by C0h, and by GigaDevice F0h. */
#define NAND_ECC_2BIT_FIELD(reg)        (((reg) >> 4) & 0b11u)

/** @brief Extract the 3-bit ECC status field (bits [6:4]) used by C0h. */
#define NAND_ECC_3BIT_FIELD(reg)        (((reg) >> 4) & 0b111u)

/** @brief 2-bit ECC status values with the same meaning on every vendor. 01b and 11b differ. */
#define NAND_ECC_2BIT_NO_ERROR          0b00
#define NAND_ECC_2BIT_UNCORRECTABLE     0b10

/**
 * @brief Worst-case corrected-bit count for a status class (range ceiling).
 *
 * Refresh compares this to the tunable threshold. Using max (not min) may
 * refresh early if the threshold sits inside a range; using min can skip a
 * needed refresh.
 *
 * @return Bits corrected (0 for NAND_ECC_OK), or NAND_ECC_BITS_UNKNOWN when no
 *         correction count applies (not corrected, invalid, or not a status).
 */
static inline uint8_t nand_ecc_max_bits_corrected(nand_ecc_status_t status)
{
    switch (status) {
    case NAND_ECC_1_TO_3_BITS_CORRECTED:
        return 3;
    case NAND_ECC_4_TO_6_BITS_CORRECTED:
        return 6;
    case NAND_ECC_7_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_1_TO_4_BITS_CORRECTED:
        return 4;
    case NAND_ECC_1_TO_7_BITS_CORRECTED:
        return 7;
    case NAND_ECC_1_BIT_CORRECTED:
        return 1;
    case NAND_ECC_2_BITS_CORRECTED:
        return 2;
    case NAND_ECC_3_BITS_CORRECTED:
        return 3;
    case NAND_ECC_4_BITS_CORRECTED:
        return 4;
    case NAND_ECC_5_BITS_CORRECTED:
        return 5;
    case NAND_ECC_6_BITS_CORRECTED:
        return 6;
    case NAND_ECC_7_BITS_CORRECTED:
        return 7;
    case NAND_ECC_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_5_TO_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_OK:
        return 0;
    case NAND_ECC_NOT_CORRECTED:
    case NAND_ECC_UNKNOWN:
    case NAND_ECC_INVALID:
    case NAND_ECC_MAX:
        return NAND_ECC_BITS_UNKNOWN;
    }
    /* No default: above, so -Wswitch flags any nand_ecc_status_t value missing here. */
    return NAND_ECC_BITS_UNKNOWN;
}

/**
 * @brief Check whether the last read's corrected-bit count calls for a data refresh.
 *
 * Statuses without a bit count (not corrected, invalid, unknown) never exceed the threshold.
 *
 * @param ecc  ECC data holding the last status and the refresh threshold.
 * @return true if the corrected-bit count meets or exceeds the threshold.
 */
static inline bool nand_ecc_exceeds_data_refresh_threshold(const nand_ecc_data_t *ecc)
{
    uint8_t bits = nand_ecc_max_bits_corrected(ecc->ecc_corrected_bits_status);
    return bits != NAND_ECC_BITS_UNKNOWN && bits >= ecc->ecc_data_refresh_threshold;
}

/**
 * @brief Operations on a vendor ECC extension register that refines some C0h values, e.g.
 *        into an exact corrected-bit count.
 *
 * Only read touches the SPI bus; needs_read and decode are pure functions the host tests
 * call directly.
 */
typedef struct {
    /** True if this C0h value must be refined from the extension register. */
    bool (*needs_read)(uint8_t status_c0);
    /** Read the extension register byte. */
    esp_err_t (*read)(spi_nand_flash_device_t *dev, uint8_t *reg_value);
    /** Map the extension register byte to a status. Only called with a value returned by read. */
    nand_ecc_status_t (*decode)(uint8_t reg_value);
} nand_ecc_ext_reg_ops_t;

/**
 * @brief Per-chip ECC status decoder, installed as spi_nand_flash_device_t::ecc_decoder.
 *
 * After a page read, the driver reads C0h. If ext_reg_ops is set and
 * ext_reg_ops->needs_read(C0h) is true, it reads the extension register and calls
 * ext_reg_ops->decode(); otherwise it calls decode(C0h).
 */
typedef struct {
    /** Map C0h to a status. Never NULL; not called when ext_reg_ops->needs_read(C0h) is true. */
    nand_ecc_status_t (*decode)(uint8_t status_c0);
    /** NULL for chips that decode from C0h alone. */
    const nand_ecc_ext_reg_ops_t *ext_reg_ops;
} nand_ecc_decoder_t;

/*
 * Pure decoders for chips that report ECC status in C0h alone. Each takes the raw C0h byte.
 *
 * Naming:
 *   Nbit     decodes the N-bit ECCS field in C0h alone, in a layout any vendor may reuse.
 *   maxN     the chip's internal ECC corrects up to N bits per ECC step.
 *   default  strength-agnostic fallback for chips that install no decoder.
 * Decoders for a vendor-only C0h layout or a vendor-specific register live in that vendor's
 * own file (e.g. nand_gigadevice_ecc_decode.c, nand_xtx_ecc_decode.c), not here.
 */

/** @brief 2-bit ECCS (C0h [5:4]): 00 OK, 01 1-3, 10 not corrected, 11 4-6. Default decoder. */
nand_ecc_status_t nand_ecc_decode_2bit_default(uint8_t status_c0);

/** @brief 3-bit ECCS (C0h [6:4]). Reserved patterns 100b, 110b, 111b map to NAND_ECC_INVALID. */
nand_ecc_status_t nand_ecc_decode_3bit(uint8_t status_c0);

/** @brief 2-bit ECCS, 4-bit strength: 01b is 1-3 corrected (no count), 11b is exactly 4. */
nand_ecc_status_t nand_ecc_decode_2bit_max4(uint8_t status_c0);

/** @brief 2-bit ECCS, 8-bit strength: 01b is 1-7 corrected (no count), 11b is exactly 8. */
nand_ecc_status_t nand_ecc_decode_2bit_max8(uint8_t status_c0);

/**
 * @brief 2-bit ECCS, 1-bit (Hamming) strength: 01b is exactly 1. 10b and 11b are both
 *        NAND_ECC_NOT_CORRECTED (Winbond W25N512G, W25N01GV/GW/JW: 11b is a 2-bit error in
 *        continuous read mode).
 */
nand_ecc_status_t nand_ecc_decode_2bit_max1(uint8_t status_c0);

/**
 * @brief Do not interpret C0h ECC bits (generic / unknown chip path). Always NAND_ECC_UNKNOWN;
 *        a successful page read means the SPI path completed, not that data is ECC-clean.
 */
nand_ecc_status_t nand_ecc_decode_unknown(uint8_t status_c0);

/* Decoder descriptors for the pure decoders above (no extra register read). */
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_default;
extern const nand_ecc_decoder_t nand_ecc_decoder_3bit;
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_max1;
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_max4;
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_max8;
extern const nand_ecc_decoder_t nand_ecc_decoder_unknown;

/**
 * @brief Check whether a 2-bit ECCS field (C0h bits [5:4]) reports a correction (01b or 11b).
 *
 * For chips that report the exact count in a separate register (Macronix ECCSR, Winbond KV
 * MBF), this is nand_ecc_ext_reg_ops_t::needs_read: 01b and 11b only differ by a vendor threshold.
 *
 * @param status_c0  Raw C0h status byte.
 * @return true if ECCS is 01b or 11b.
 */
bool nand_ecc_2bit_reports_correction(uint8_t status_c0);

/**
 * @brief Map an exact corrected-bit count read from a vendor register to a status.
 *
 * 0 -> NAND_ECC_OK, 1-8 -> exact count, 1111b (>8) -> NAND_ECC_NOT_CORRECTED, undefined
 * (1001b-1110b) -> NAND_ECC_INVALID. Used for Macronix ECCSR[3:0] and Winbond KV MBF
 * (register 30h [7:4]).
 *
 * @param count  4-bit error count, already extracted from the vendor register.
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_ecc_status_from_count(uint8_t count);

/**
 * @brief Decode the ECC status of the page just read and record it in
 *        dev->chip.ecc_data.ecc_corrected_bits_status, using dev->ecc_decoder.
 *
 * Reads the decoder's extension register if it needs one. dhara_glue.c relies on ESP_FAIL from
 * nand_read()/nand_copy() meaning exactly this ECC verdict.
 *
 * @param dev        Device handle; dev->ecc_decoder must be set.
 * @param status_c0  Raw C0h status byte read after the page load.
 * @return ESP_OK if the data is usable, ESP_FAIL on an uncorrectable or invalid status, or the
 *         extension register read error unchanged (the recorded status is then NAND_ECC_INVALID).
 */
esp_err_t nand_ecc_check_status(spi_nand_flash_device_t *dev, uint8_t status_c0);

#ifdef __cplusplus
}
#endif
