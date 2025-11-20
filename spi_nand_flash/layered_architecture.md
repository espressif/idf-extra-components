# SPI NAND Flash Layered Architecture

This document describes the layered architecture implemented in the spi_nand_flash component, designed to provide cleaner separation of concerns, better maintainability, and enhanced extensibility while maintaining full backward compatibility.

## Architecture Overview

### Layered Structure

```
Application/Filesystem
     ↓
┌────────────────────────────────────────┐
│ spi_nand_flash.h (Public API)         │ ← Backward Compatible Interface
│ - spi_nand_flash_init_device()        │
│ - spi_nand_flash_read_sector()        │
│ - spi_nand_flash_write_sector()       │
└────────────────────────────────────────┘
     ↓
┌────────────────────────────────────────┐
│ NAND Wear-Leveling BDL                 │ ← Logical Sector Management
│ (dhara_glue.c, nand_wl_blockdev.c)    │
│ - Logical-to-physical mapping          │
│ - Wear leveling (Dhara integration)    │
│ - Bad block management                 │
│ - Garbage collection                   │
└────────────────────────────────────────┘
     ↓
┌────────────────────────────────────────┐
│ NAND Flash BDL                         │ ← Physical Flash Operations
│ (nand_flash_blockdev.c, nand_impl.c)  │
│ - Physical block operations            │
│ - Device-specific implementations      │
│ - ECC error handling                   │
│ - Direct hardware access               │
└────────────────────────────────────────┘
     ↓
┌────────────────────────────────────────┐
│ SPI NAND Operations / Emulation        │ ← Hardware Abstraction
│ (spi_nand_oper.c / nand_linux_mmap_   │
│  emul.c)                               │
│ - SPI transaction handling (ESP)       │
│ - Memory-mapped file emulation (Linux) │
│ - Command execution                    │
│ - Register access                      │
└────────────────────────────────────────┘
```

### Key Improvements

1. **Clear Separation of Concerns**
   - **Wear-Leveling Layer**: Handles logical-to-physical address mapping, wear leveling, and high-level bad block management
   - **Flash Layer**: Manages physical flash operations, device detection, and low-level error handling
   - **SPI Layer**: Handles SPI communication and command execution

2. **Better Maintainability**
   - Each layer has well-defined interfaces
   - Reduced coupling between components
   - Easier to test individual layers (Linux emulation support)
   - Clear header organization

3. **Enhanced Extensibility**
   - Easy to add new wear-leveling algorithms
   - Support for different NAND flash types
   - Better error handling and recovery

## Header File Organization

### Public Headers (`include/`)

- **`spi_nand_flash.h`** - Main public API (backward compatible)
  - Legacy functions: `spi_nand_flash_init_device()`, `spi_nand_flash_read_sector()`, etc.
  - Layered API: `spi_nand_flash_init_with_layers()` for direct layer access

- **`nand_device_types.h`** - Common types and definitions
  - `nand_ecc_status_t` - ECC status enumeration
  - `nand_device_info_t` - Device identification (manufacturer, device ID, chip name)
  - `nand_flash_geometry_t` - Flash geometry (page size, block size, ECC data)
  - `nand_flash_info_t` - Complete flash information

- **`esp_nand_blockdev.h`** - Block device interface
  - Block device creation functions
  - NAND-specific ioctl commands
  - Argument structures for ioctl operations

### Private Headers (`priv_include/`)

- **`nand.h`** - Internal device structure and operations
  - `spi_nand_flash_device_t` - Main device handle
  - `nand_wl_attach_ops()` / `nand_wl_detach_ops()` - Dhara integration

- **`nand_impl.h`** - Low-level flash operations
  - `nand_init_device()` - Internal device initialization
  - Page read/write/erase functions
  - Bad block management
  - ECC status handling

- **`nand_flash_devices.h`** - Device identification and initialization
  - Manufacturer IDs and device IDs
  - Device-specific initialization functions

- **`spi_nand_oper.h`** - SPI operations (ESP targets only)
  - SPI transaction functions
  - Register read/write operations

- **`nand_linux_mmap_emul.h`** - Linux emulation (host testing)
  - `nand_init_device()` - Internal device initialization
  - Memory-mapped file emulation
  - NAND flash simulation for testing

## Source File Organization

### Core Implementation (`src/`)

```
src/
├── nand.c                      # Public API implementation
│                               # - spi_nand_flash_init_device()
│                               # - spi_nand_flash_read_sector()
│                               # - spi_nand_flash_write_sector()
│
├── nand_impl.c                 # Flash BDL implementation
│                               # - Device detection and initialization
│                               # - nand_read(), nand_prog(), nand_erase()
│                               # - nand_is_bad(), nand_mark_bad()
│                               # - nand_is_free()
│
├── nand_flash_blockdev.c       # Flash BDL block device adapter
│                               # - nand_flash_get_blockdev()
│                               # - Ioctl command handling
│
├── dhara_glue.c                # Wear-Leveling BDL implementation
│                               # - Dhara library integration
│                               # - nand_wl_attach_ops() / nand_wl_detach_ops()
│                               # - Logical-to-physical mapping
│
├── nand_wl_blockdev.c          # WL BDL block device adapter
│                               # - spi_nand_flash_wl_get_blockdev()
│                               # - Wear-leveling operations
│                               # - Sector read/write/trim
│
├── nand_diag_api.c             # Diagnostic and statistics API
│
├── spi_nand_oper.c             # SPI operations (ESP targets)
│                               # - SPI transaction handling
│
├── nand_linux_mmap_emul.c      # Linux emulation (host testing)
│                               # - Memory-mapped file I/O
│
└── devices/                    # Device-specific implementations
    ├── nand_winbond.c
    ├── nand_gigadevice.c
    ├── nand_alliance.c
    ├── nand_micron.c
    ├── nand_zetta.c
    └── nand_xtx.c
```

## API Usage

### Backward Compatible API (Recommended for existing projects)

```c
#include "spi_nand_flash.h"

// Existing code continues to work unchanged
spi_nand_flash_config_t config = {
    .device_handle = spi_handle,
    .spi_nand_mode_cfg_id = 0,
    .io_mode = SPI_NAND_IO_MODE_QIO,
    .freq_mhz = 20,
};

spi_nand_flash_device_t *handle;

// Initialize device
esp_err_t ret = spi_nand_flash_init_device(&config, &handle);

// Read/write sectors
uint8_t buffer[2048];
ret = spi_nand_flash_read_sector(handle, buffer, sector_id);
ret = spi_nand_flash_write_sector(handle, buffer, sector_id);

// Cleanup
spi_nand_flash_deinit_device(handle);
```

### New Layered API (Advanced users)

```c
#include "spi_nand_flash.h"
#include "esp_nand_blockdev.h"

// Initialize with separate layer handles
spi_nand_flash_config_t config = {...};
esp_blockdev_handle_t flash_bdl;
esp_blockdev_handle_t wl_bdl;

// Create Flash Block Device Layer
esp_err_t ret = nand_flash_get_blockdev(&config, &flash_bdl);

// Create Wear-Leveling Block Device Layer
ret = spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl);

// Use block device operations
uint8_t buffer[2048];
wl_bdl->ops->read(wl_bdl, buffer, sector_size, offset, size);
wl_bdl->ops->write(wl_bdl, buffer, offset, size);

// Check bad blocks using ioctl
esp_blockdev_cmd_arg_status_t cmd = { .num = block_num };
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &cmd);
if (cmd.status) {
    printf("Block %u is bad\n", block_num);
}

// Get flash information
esp_blockdev_nand_flash_info_t flash_info;
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO, &flash_info);
printf("Chip: %s, Size: %u blocks\n", 
       flash_info.device_info.chip_name,
       flash_info.geometry.num_blocks);

// Cleanup (releases both WL and Flash layers)
wl_bdl->ops->release(wl_bdl);
```

### Combined Initialization (Simplified)

```c
#include "spi_nand_flash.h"

spi_nand_flash_config_t config = {...};
esp_blockdev_handle_t flash_bdl;
esp_blockdev_handle_t wl_bdl;

// Initialize both layers at once
esp_err_t ret = spi_nand_flash_init_with_layers(&config, 
                                                &flash_bdl, 
                                                &wl_bdl);

// Use filesystem
esp_vfs_fat_sdmmc_mount_config_t mount_config = {...};
ret = esp_vfs_fat_spiflash_mount_rw_wl("/data", NULL, &mount_config, &wl_bdl);
```

## Block Device IOCTL Commands

### Flash BDL Commands

| Command | Description | Argument Type |
|---------|-------------|---------------|
| `ESP_BLOCKDEV_CMD_IS_BAD_BLOCK` | Check if block is bad | `esp_blockdev_cmd_arg_status_t` |
| `ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK` | Mark block as bad | `uint32_t*` (block number) |
| `ESP_BLOCKDEV_CMD_IS_FREE_PAGE` | Check if page is free | `esp_blockdev_cmd_arg_status_t` |
| `ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS` | Get ECC status | `esp_blockdev_cmd_arg_ecc_status_t` |
| `ESP_BLOCKDEV_CMD_GET_BAD_BLOCK_COUNT` | Get bad block count | `uint32_t*` |
| `ESP_BLOCKDEV_CMD_GET_ECC_STATS` | Get ECC statistics | `esp_blockdev_nand_ecc_status_t*` |
| `ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO` | Get device info + geometry | `esp_blockdev_nand_flash_info_t*` |

### WL BDL Commands

| Command | Description | Argument Type |
|---------|-------------|---------------|
| `ESP_BLOCKDEV_CMD_GET_AVAIL_SECTORS` | Get available sectors | `uint32_t*` |
| `ESP_BLOCKDEV_CMD_TRIM_SECTOR` | Trim/discard sector | `uint32_t*` (sector number) |

## Testing and Validation

### Linux Host Testing

The component supports host-based testing on Linux using memory-mapped file emulation:

```c
#ifdef CONFIG_IDF_TARGET_LINUX
#include "nand_linux_mmap_emul.h"

// Configure emulation
nand_file_mmap_emul_config_t emul_cfg = {
    .flash_file_name = "",              // Empty = auto-generate temp file
    .flash_file_size = 50 * 1024 * 1024, // 50MB
    .keep_dump = false                   // Delete file on cleanup
};

spi_nand_flash_config_t config = {
    .device_handle = &emul_cfg,
    // ... other config ...
};

// Use normally - will emulate NAND flash
spi_nand_flash_device_t *handle;
spi_nand_flash_init_device(&config, &handle);
#endif
```

See `host_test/README.md` for more details on running tests.

## Migration Guide

### For Existing Projects

**No changes required!** The existing API is fully supported and will continue to work. The implementation now uses the layered architecture internally, providing better reliability and maintainability.

### For New Projects

Consider using the layered API if you need:
- Direct access to flash operations (bypassing wear leveling)
- NAND-specific operations (bad block management, ECC status)
- Detailed statistics and monitoring
- Integration with filesystems via `esp_blockdev_t` interface

## Error Handling

The layered architecture provides enhanced error reporting:

- **Flash Layer**: Reports hardware-level errors, ECC failures, bad blocks
- **Wear-Leveling Layer**: Reports mapping errors, garbage collection issues
- **Public API**: Maintains backward-compatible error codes

### Internal Improvements

While the API remains the same, the internal implementation has been refactored:
- Better code organization
- Enhanced error handling
- Improved modularity

---

This layered architecture provides a foundation for future development while maintaining full compatibility with existing code. The modular design makes the component more maintainable, testable, and extensible.
