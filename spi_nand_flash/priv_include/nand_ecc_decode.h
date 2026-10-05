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
#define NAND_ECC_2BIT_FIELD(reg)        (((reg) >> 4) & 0x3u)

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
    case NAND_ECC_OK:
        return 0;
    case NAND_ECC_NOT_CORRECTED:
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
 * Statuses without a bit count (not corrected, invalid) never exceed the threshold.
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
 * @brief Per-chip ECC status decoder, installed as spi_nand_flash_device_t::ecc_decoder.
 *
 * Decoding is split so that only read_extra touches the SPI bus; decode is a pure function
 * that the host tests call directly. After a page read, the driver:
 *   1. reads C0h,
 *   2. if read_extra is set and needs_extra_read(C0h) is true (or needs_extra_read is NULL),
 *      calls read_extra to fetch one vendor register byte,
 *   3. calls decode(C0h, extra), with extra = 0 when no read was made.
 */
typedef struct {
    /** Whether this C0h value needs the extra register. NULL means always, when read_extra is set. */
    bool (*needs_extra_read)(uint8_t status_c0);
    /** Read one vendor register byte. NULL for chips that decode from C0h alone. */
    esp_err_t (*read_extra)(spi_nand_flash_device_t *dev, uint8_t *extra);
    /** Map C0h and the extra byte to a status. Never NULL. */
    nand_ecc_status_t (*decode)(uint8_t status_c0, uint8_t extra);
} nand_ecc_decoder_t;

/*
 * Pure decoders for chips that report ECC status in C0h alone. Each takes the raw C0h byte;
 * `extra` is unused and present only to match nand_ecc_decoder_t::decode.
 * The strength_N suffix is the number of bits the chip's internal ECC can correct per step.
 */

/** @brief 2-bit ECCS (C0h [5:4]): 00 OK, 01 1-3, 10 not corrected, 11 4-6. Default decoder. */
nand_ecc_status_t nand_ecc_decode_2bit(uint8_t status_c0, uint8_t extra);

/** @brief 3-bit ECCS (C0h [6:4]). Reserved patterns 100b, 110b, 111b map to NAND_ECC_INVALID. */
nand_ecc_status_t nand_ecc_decode_3bit(uint8_t status_c0, uint8_t extra);

/** @brief 2-bit ECCS, 4-bit strength: 01b is 1-3 corrected (no count), 11b is exactly 4. */
nand_ecc_status_t nand_ecc_decode_2bit_strength_4(uint8_t status_c0, uint8_t extra);

/** @brief 2-bit ECCS, 8-bit strength: 01b is 1-7 corrected (no count), 11b is exactly 8. */
nand_ecc_status_t nand_ecc_decode_2bit_strength_8(uint8_t status_c0, uint8_t extra);

/**
 * @brief 2-bit ECCS, 1-bit (Hamming) strength: 01b is exactly 1. 10b and 11b are both
 *        NAND_ECC_NOT_CORRECTED (Winbond W25N512G, W25N01GV/GW/JW: 11b is a 2-bit error in
 *        continuous read mode).
 */
nand_ecc_status_t nand_ecc_decode_2bit_strength_1(uint8_t status_c0, uint8_t extra);

/**
 * @brief XTX XT26G08D 4-bit ECCS (C0h [7:4]). ECCS1:0: 00b OK, 10b not corrected, 11b
 *        exactly 8. When ECCS1:0 is 01b, ECCS3:2 gives the count: <=4, 5, 6 or 7.
 */
nand_ecc_status_t nand_ecc_decode_xtx(uint8_t status_c0, uint8_t extra);

/* Decoder descriptors for the pure decoders above (no extra register read). */
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit;
extern const nand_ecc_decoder_t nand_ecc_decoder_3bit;
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_strength_1;
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_strength_4;
extern const nand_ecc_decoder_t nand_ecc_decoder_2bit_strength_8;
extern const nand_ecc_decoder_t nand_ecc_decoder_xtx;

/**
 * @brief Check whether a 2-bit ECCS field (C0h bits [5:4]) reports a correction (01b or 11b).
 *
 * For chips that report the exact count in a separate register (Macronix ECCSR, Winbond KV
 * MBF), this tells the caller whether that register needs to be read.
 *
 * @param status_c0  Raw C0h status byte.
 * @return true if ECCS is 01b or 11b.
 */
bool nand_ecc_2bit_reports_correction(uint8_t status_c0);

/**
 * @brief Decode a 2-bit ECCS field (C0h bits [5:4]) together with an exact error count read
 *        from a vendor register.
 *
 * ECCS 00b: no errors; 10b: not correctable; `count` is not used. ECCS 01b/11b: `count` is
 * the trusted source: 0 -> NAND_ECC_OK, 1-8 -> exact count, 1111b (>8) ->
 * NAND_ECC_NOT_CORRECTED, undefined (1001b-1110b) -> NAND_ECC_INVALID. Used for Macronix
 * ECCSR[3:0] and Winbond KV MBF (register 30h [7:4]).
 *
 * @param status_c0  Raw C0h status byte.
 * @param count      4-bit error count, already extracted from the vendor register (don't-care
 *                   unless nand_ecc_2bit_reports_correction() is true).
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_ecc_decode_2bit_with_count(uint8_t status_c0, uint8_t count);

#ifdef __cplusplus
}
#endif
