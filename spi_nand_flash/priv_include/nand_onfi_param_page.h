/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN      4
#define NAND_ONFI_PARAM_PAGE_MANUFACTURER_LEN   12
#define NAND_ONFI_PARAM_PAGE_MODEL_LEN          20
#define NAND_ONFI_PARAM_PAGE_SIZE               256
#define NAND_ONFI_PARAM_PAGE_COPIES             3

/** Conservative tR ceiling (µs) while waiting during parameter-page PAGE_READ before geometry is known. */
#define NAND_ONFI_PARAM_PAGE_READ_WAIT_US       1000

/**
 * ONFI parameter page (Table 16, ONFI 1.0 §5.4.1).
 * Multi-byte fields are little-endian; first byte is least significant.
 */
typedef struct __attribute__((packed))
{
    /* Revision information and features block (bytes 0-31) */
    uint8_t  signature[NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN];                                    /* bytes 0-3: Parameter page signature */
    uint16_t revision_number;                                 /* bytes 4-5: Revision number */
    uint16_t features_supported;                              /* bytes 6-7: Features supported */
    uint16_t optional_commands_supported;                     /* bytes 8-9: Optional commands supported */
    uint8_t  reserved_10_31[22];                              /* bytes 10-31: Reserved (0) */
    /* Manufacturer information block (bytes 32-79) */
    char     manufacturer[NAND_ONFI_PARAM_PAGE_MANUFACTURER_LEN]; /* bytes 32-43: Device manufacturer (12 ASCII) */
    char     model[NAND_ONFI_PARAM_PAGE_MODEL_LEN];               /* bytes 44-63: Device model (20 ASCII) */
    uint8_t  jedec_id;                                        /* byte 64: JEDEC manufacturer ID */
    uint16_t date_code;                                       /* bytes 65-66: Date code */
    uint8_t  reserved_67_79[13];                              /* bytes 67-79: Reserved (0) */
    /* Memory organization block (bytes 80-127) */
    uint32_t data_bytes_per_page;                             /* bytes 80-83: Number of data bytes per page */
    uint16_t spare_bytes_per_page;                            /* bytes 84-85: Number of spare bytes per page */
    uint32_t data_bytes_per_partial_page;                     /* bytes 86-89: Number of data bytes per partial page */
    uint16_t spare_bytes_per_partial_page;                    /* bytes 90-91: Number of spare bytes per partial page */
    uint32_t pages_per_block;                                 /* bytes 92-95: Number of pages per block */
    uint32_t blocks_per_lun;                                  /* bytes 96-99: Number of blocks per LUN */
    uint8_t  num_luns;                                        /* byte 100: Number of logical units (LUNs) */
    uint8_t  num_address_cycles;                              /* byte 101: Number of address cycles */
    uint8_t  bits_per_cell;                                   /* byte 102: Number of bits per cell */
    uint16_t bad_blocks_max_per_lun;                          /* bytes 103-104: Bad blocks maximum per LUN */
    uint16_t block_endurance;                                 /* bytes 105-106: Block endurance */
    uint8_t  guaranteed_valid_blocks;                         /* byte 107: Guaranteed valid blocks at beginning of target */
    uint16_t guaranteed_block_endurance;                      /* bytes 108-109: Block endurance for guaranteed valid blocks */
    uint8_t  programs_per_page;                               /* byte 110: Number of programs per page */
    uint8_t  partial_programming_attributes;                  /* byte 111: Partial programming attributes */
    uint8_t  ecc_correctability;                              /* byte 112: Number of bits ECC correctability */
    /* Bytes 113–114: kept so the packed struct matches the 256-byte ONFI
     * page; unused in v1. Not the same as nand_impl.c plane-select. */
    uint8_t  num_interleaved_address_bits;                    /* byte 113: Number of interleaved address bits */
    uint8_t  interleaved_operation_attributes;                /* byte 114: Interleaved operation attributes */
    uint8_t  reserved_115_127[13];                            /* bytes 115-127: Reserved (0) */
    /* Electrical parameters block (bytes 128-163) */
    uint8_t  io_pin_capacitance;                              /* byte 128: I/O pin capacitance */
    uint16_t timing_mode_support;                             /* bytes 129-130: Timing mode support */
    uint16_t program_cache_timing_mode_support;               /* bytes 131-132: Program cache timing mode support */
    uint16_t t_prog_max_us;                                   /* bytes 133-134: tPROG maximum page program time (µs) */
    uint16_t t_bers_max_us;                                   /* bytes 135-136: tBERS maximum block erase time (µs) */
    uint16_t t_r_max_us;                                      /* bytes 137-138: tR maximum page read time (µs) */
    uint16_t t_ccs_min_ns;                                    /* bytes 139-140: tCCS minimum change column setup time (ns) */
    uint8_t  reserved_141_163[23];                            /* bytes 141-163: Reserved (0) */
    /* Vendor block (bytes 164-255) */
    uint16_t vendor_revision;                                 /* bytes 164-165: Vendor specific revision number */
    uint8_t  vendor_specific[88];                           /* bytes 166-253: Vendor specific */
    uint16_t crc;                                             /* bytes 254-255: Integrity CRC */
} nand_parameter_page_t;

_Static_assert(sizeof(nand_parameter_page_t) == NAND_ONFI_PARAM_PAGE_SIZE,
               "nand_parameter_page_t must match ONFI parameter page size");

#ifdef __cplusplus
}
#endif
