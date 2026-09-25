# ECC decode checklist, non-GigaDevice chips

This file tracks the ECC status decoding check for each supported chip. Each row is one
`case` in a driver's `switch (device_id)`. GigaDevice is done in `refactor/gd-eccse-decoder`.

For each ID:
1. Confirm the part number and device ID in the datasheet's Read ID table.
2. Write down the internal ECC strength (bits per sector) and the ECC status bits (C0h, plus
   any extra register or command).
3. Check the current decoder against the datasheet's ECC status table. Pick a shared decoder
   or add a vendor decoder.
4. Update the `case` comment in the driver in the same format as GigaDevice:
   `// <part number> (<voltage>) - <N> bits/<sector>B ECC strength`.

Shared decoders (`src/nand_ecc_decode.c`):
- 2b: `nand_ecc_decode_2bit`, the default from `nand_impl.c` (01 -> 1-3, 11 -> 4-6)
- 3b: `nand_ecc_decode_3bit` (001 -> 1-3, 011 -> 4-6, 101 -> 7-8, 100/110/111 -> invalid)
- `nand_ecc_decode_2bit_1bit_strength`: 01 -> exactly 1, 10 and 11 -> uncorrectable
- `nand_ecc_decode_2bit_4bit_strength`: 01 -> 1-3, 11 -> exactly 4
- `nand_ecc_decode_2bit_8bit_strength`: 01 -> 1-7, 11 -> exactly 8
- `nand_ecc_decode_xtx`: C0h [7:4]

Vendor decoders with an extra register read: `mx_ecc_decode` (Macronix, 7Ch) and
`wb_kv_ecc_decode` (Winbond KV, register 30h). Their bit decoding is in
`src/devices/nand_<vendor>_ecc_decode.c`.

## Gaps and TODOs

### Before the PR

- [ ] Build with ESP-IDF. Nothing on this branch has been built with `idf.py` yet: only the
      decode files were compiled on their own with a small C check. The local IDF Python env
      needs `install.sh` (`idf_drivers_gdb` missing, `esp-idf-sbom` too old).
- [ ] Run the host tests (`host_test`, target linux) and build at least one real target. The
      driver files and the new 7Ch / 30h reads have never been compiled.
- [ ] Try the extra register reads on hardware: a Macronix MX35LF chip (7Ch) and a Winbond
      W25N02KV/04KV (register 30h).
- [ ] Rebase on master once the GigaDevice PR (`refactor/gd-eccse-decoder`) is merged.
- [ ] Decide whether this checklist stays in the repo or is dropped before merge.

### ECC decoding

- [ ] Micron: go through all four IDs again, one by one, against their own datasheets. The
      first pass was not done per ID (see the Micron section).
- [ ] GigaDevice 0x35 / 0x25: part number still unidentified (TODO from the base branch). They
      stay on the default 2b decoder.
- [ ] Macronix: CONT (continuous read) is assumed to be 0 and the driver never sets it. With
      CONT=1, both C0h and ECCSR report accumulated pages. Check the default and where the bit
      is (Linux: config register bit 2).
- [ ] Macronix: BFT (bit flip threshold) feature address and default not written down. The
      decoder does not need it.
- [ ] FMSH FM25S005BI3: the datasheet calls the internal ECC an "option". Check whether the
      driver sets the ECC-enable bit (B0h) or relies on the power-on default.
- [ ] Alliance: both datasheets say the 11b maximum is "according to extended register", but
      neither describes such a register. Treated as boilerplate; revisit if one turns up.

### Not ECC (separate fixes)

- [ ] Winbond xxxT parts (W25N01GVxxxT and others) power up with BUF=0 (continuous read), and
      the driver never sets BUF=1. Read commands differ between the modes, and ECC status is
      per operation instead of per page. Likely fix: set BUF=1 at init for Winbond.
- [ ] Winbond W25N04KV (0xAA23): the datasheet says page data read from one plane cannot be
      programmed to the other plane, which is what `nand_copy()` does. It has no plane address
      bits (parameter page byte 113 = 00h), so the plane-select flags are correctly unset. It may
      need `NAND_FLAG_IDM_SAME_PARITY_REQUIRED`, but the datasheet does not say how blocks map to
      planes (Linux assumes odd/even) and has no IDM section.
- [ ] Micron 0x35 (1.8 V MT29F4G01ABBFD) and 0x36 / 0x46 (MT29F4G01ADAGD / MT29F8G01ADAFD) are
      known IDs that the driver does not support. Not planned; noted only.

## Winbond (`src/devices/nand_winbond.c`), decoder per ID

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [x] | 0xAA20 | W25N512GVxxG/T/R (MID EFh) | 3.3 V | 1 bit/528B (Hamming) | same as 0xAA21 | `nand_ecc_decode_2bit_1bit_strength` | Not in Linux |
| [x] | 0xBA20 | W25N512GWxxR/T (MID EFh) | 1.8 V | 1 bit/528B (Hamming) | same as 0xAA21 | `nand_ecc_decode_2bit_1bit_strength` | |
| [x] | 0xAA21 | W25N01GVxxxG/T/R (MID EFh) | 3.3 V | 1 bit/528B (Hamming) | C0h [5:4]: 00 OK, 01 1 bit corrected, 10 2-bit error single page, 11 2-bit errors multiple pages (continuous read only) | `nand_ecc_decode_2bit_1bit_strength` | xxxT defaults to BUF=0 (see TODOs) |
| [x] | 0xBA21 | W25N01GWxxxG/T (MID EFh) | 1.8 V | 1 bit/528B (Hamming) | same as 0xAA21 | `nand_ecc_decode_2bit_1bit_strength` | Added to README |
| [x] | 0xBC21 | W25N01JWxxxG/T (MID EFh) | 1.8 V | 1 bit/528B (Hamming) | same as 0xAA21 | `nand_ecc_decode_2bit_1bit_strength` | Matches Linux `w25w35nxxjw_ecc_get_status` |
| [x] | 0xAA22 | W25N02KVxxIR/U (MID EFh) | 3.3 V | 8 bits/528B | C0h [5:4]: 00 OK, 01 corrected <= BFD, 10 uncorrectable, 11 corrected > BFD. 30h: MBF [7:4] max bit flips per sector (0-8, 1111 = >8), MFS [2:0] sector | `wb_kv_ecc_decode`, exact count from 30h | BFD in 10h [7:4], default 4, ignored. 30h read with 0Fh + address (datasheet "Read Extended Internal ECC feature registers"), same as Linux |
| [x] | 0xAA23 | W25N04KVxxIR/U (MID EFh) | 3.3 V | 8 bits/528B | same as 0xAA22 | `wb_kv_ecc_decode` | Cross-plane copy restriction (see TODOs) |

## Alliance (`src/devices/nand_alliance.c`), decoder per ID

C0h [5:4] for all IDs: 00 OK, 01 corrected (no count), 10 uncorrectable, 11 corrected = ECC max.

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [x] | 0x25 | AS5F31G04SND-08LIN (MID 52h) | 3.3 V | 4 bits/528B | see above | `nand_ecc_decode_2bit_4bit_strength` | 2048+64 B pages |
| [x] | 0x2E | AS5F32G04SND-08LIN (MID 52h) | 3.3 V | 8 bits/544B | see above | `nand_ecc_decode_2bit_8bit_strength` | 2048+128 B pages |
| [x] | 0x8E | AS5F12G04SND-10LIN (MID 52h) | 1.8 V | 8 bits/544B | see above | `nand_ecc_decode_2bit_8bit_strength` | 2048+128 B pages |
| [x] | 0x2F | AS5F34G04SND-08LIN (MID 52h) | 3.3 V | 8 bits/544B | see above | `nand_ecc_decode_2bit_8bit_strength` | 2048+128 B pages. Linux lists ECCREQ(4, 512), which contradicts the datasheet; its OOB-size rule still decodes it as 8-bit |
| [x] | 0x8F | AS5F14G04SND-10LIN (MID 52h) | 1.8 V | 8 bits/544B | see above | `nand_ecc_decode_2bit_8bit_strength` | 2048+128 B pages |
| [x] | 0x2D | AS5F38G04SND-08LIN (MID 52h) | 3.3 V | 8 bits/544B | see above | `nand_ecc_decode_2bit_8bit_strength` | 4096+256 B pages |
| [x] | 0x8D | AS5F18G04SND-10LIN (MID 52h) | 1.8 V | 8 bits/544B | see above | `nand_ecc_decode_2bit_8bit_strength` | 4096+256 B pages |

## Micron (`src/devices/nand_micron.c`), decoder: 3b

**Needs a second pass.** The first pass was not done per ID against each part's own
datasheet. The values below come from what was read at the time and from Linux
`drivers/mtd/nand/spi/micron.c`. Recheck every row: Read ID, ECC strength and sector size,
the C0h [6:4] table, and geometry (planes, blocks, page size). Then tick it again.

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [ ] | 0x34 | MT29F4G01ABAFDWB, MT29F4G01ABAFD12 (MID 2Ch, DID 34h) | 3.3 V | 8 bits/sector | C0h [6:4], standard 3-bit table | 3b | 4 KiB pages, 1 plane (Linux M70A). 35h is the 1.8 V variant, not in the driver |
| [ ] | 0x14 | MT29F1G01ABAFDSF, MT29F1G01ABAFD12, MT29F1G01ABAFDWB (MID 2Ch, DID 14h) | 3.3 V | 8 bits/sector | C0h [6:4], standard 3-bit table | 3b | 1 plane; plane select is a dummy bit. The 1Gb datasheet's 9Fh table also lists family IDs (2Gb 24h, 4Gb 36h, 8Gb 46h) but says nothing else about those parts |
| [ ] | 0x15 | M78A 1Gb 1.8 V (Linux names it MT29F1G01ABAFD; no datasheet checked) | 1.8 V | 8 bits/512B (Linux) | standard 3-bit (Linux `micron_8_ecc_get_status`) | 3b | Source is Linux only. Find the 1.8 V 1Gb datasheet (probably MT29F1G01ABBFD). Same geometry as 0x14 |
| [ ] | 0x24 | MT29F2G01ABAGDSF, MT29F2G01ABAGD12, MT29F2G01ABAGDWB (MID 2Ch, DID 24h) | ? | 8 bits/sector | C0h [6:4], standard 3-bit table | 3b | 2 planes with plane select in the driver (Linux M79A agrees). Voltage not written down |

## Zetta (`src/devices/nand_zetta.c`), decoder: `nand_ecc_decode_2bit_8bit_strength`

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [x] | 0x71 | ZD35Q1GC | | 8 bits/528B | C0h [5:4]: 00 OK, 01 corrected (no count), 10 uncorrectable, 11 exactly 8 corrected | `nand_ecc_decode_2bit_8bit_strength` (01 -> 1-7, 11 -> 8) | Default 2b was wrong for 01 and 11. Not in Linux |

## XTX (`src/devices/nand_xtx.c`), decoder: `nand_ecc_decode_xtx`

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [x] | 0x37 | XT26G08D | | 8 bits/528B (always on) | C0h [7:4] ECCS3:0: xx00 OK, 0001 <=4, 0101 5, 1001 6, 1101 7, xx10 uncorrectable, xx11 exactly 8 | `nand_ecc_decode_xtx` | 4 KiB pages. Matches Linux `xt26xxxd_ecc_get_status` (XT26G08D itself is not in Linux) |

## FMSH (`src/devices/nand_fm.c`), decoder: 3b

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [x] | 0xD5 | FM25S005BI3 (MID A1h, DID D5h) | 3.3 V | 8 bits/528B | C0h [6:4], standard 3-bit table (100/110/111 undefined) | 3b | Same layout as Linux FM25S01BI3. Unlike FM25G01B/02B, which have a different 3-bit table. ECC-enable bit not checked (see TODOs) |

## Macronix (`src/devices/nand_macronix.c`), decoder: `mx_ecc_decode` (C0h + 7Ch ECCSR)

| Done | ID | Part number | Voltage | ECC strength | ECC status bits / registers | Decoder | Notes |
|---|---|---|---|---|---|---|---|
| [x] | 0x26 | MX35LF2GE4AD (MID C2h, DID 26h 03h) | | 8 bits/(512+32)B | C0h [5:4]: 00 OK, 01 corrected < BFT, 10 uncorrectable, 11 corrected >= BFT. 7Ch ECCSR [3:0] current page count (0-8, 1111 = >8), [7:4] accumulated | `mx_ecc_decode`, exact count from ECCSR | BFT ignored. Assumes CONT=0 (see TODOs) |
| [x] | 0x37 | MX35LF4GE4AD (MID C2h, DID 37h 03h) | | 8 bits/(512+32)B | same as 0x26 | same as 0x26 | 4 KiB pages |
