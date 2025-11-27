# SPI NAND Flash Layered Architecture

This document describes the layered architecture implemented in the spi_nand_flash component, designed to provide cleaner separation of concerns, better maintainability, and enhanced extensibility while maintaining full backward compatibility.

## Feature Configuration

The component supports two modes of operation controlled by Kconfig:

- **Legacy Mode** (default): Traditional API only, minimal memory footprint
- **BDL Mode** (`CONFIG_NAND_FLASH_ENABLE_BDL=y`): Includes Block Device Layer support with advanced features

### Kconfig Options

#### `CONFIG_NAND_FLASH_ENABLE_BDL`
**Type:** `bool`  
**Default:** `n`  
**Description:** Enable Block Device Layer (BDL) support for SPI NAND Flash

When enabled, provides:
- Standard `esp_blockdev_t` interface for layered architecture
- Flash Block Device Layer (raw NAND flash access)
- Wear-Leveling Block Device Layer (logical sector access with wear leveling)
- BDL-based FATFS mount API (`esp_vfs_fat_nand_mount_bdl`)
- Advanced layered API (`spi_nand_flash_init_with_layers`)

When disabled, only the legacy API is available.

**Enable via menuconfig:**
```
Component config → SPI NAND Flash configuration → [*] Enable Block Device Layer (BDL) support
```

## Architecture Overview

### Layered Structure

#### Legacy Mode (Default)
```
Application
     ↓
┌────────────────────────────────────────┐
│ spi_nand_flash.h (Public API)         │ ← Legacy Interface
│ - spi_nand_flash_init_device()        │
│ - spi_nand_flash_read_sector()        │
│ - spi_nand_flash_write_sector()       │
│ - spi_nand_flash_trim()                │
│ - spi_nand_flash_sync()                │
│ - spi_nand_flash_gc()                  │
└────────────────────────────────────────┘
     ↓
┌────────────────────────────────────────┐
│ NAND Wear-Leveling Layer               │ ← Logical Sector Management
│ (dhara_glue.c)                         │
│ - Logical-to-physical mapping          │
│ - Wear leveling (Dhara integration)    │
│ - Bad block management                 │
│ - Garbage collection                   │
└────────────────────────────────────────┘
     ↓
┌────────────────────────────────────────┐
│ NAND Flash Implementation              │ ← Physical Flash Operations
│ (nand_impl.c)                          │
│ - Physical page/block operations       │
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

#### BDL Mode (CONFIG_NAND_FLASH_ENABLE_BDL=y)
```
Application/Filesystem
     ↓
┌────────────────────────────────────────┐
│ spi_nand_flash.h (Public API)         │ ← Legacy + BDL Interface
│ Legacy API + BDL API:                  │
│ - spi_nand_flash_init_with_layers()   │
└────────────────────────────────────────┘
     │
     ├─────────────────────────────────────┐
     ↓                                     ↓
┌────────────────────────────┐  ┌────────────────────────────┐
│ VFS/Filesystem Layer       │  │ Direct BDL Access          │
│ (vfs_fat_spinandflash.c)   │  │ (Application)              │
│ - esp_vfs_fat_nand_mount() │  │ - nand_flash_get_blockdev()│
│ - esp_vfs_fat_nand_mount_  │  │ - spi_nand_flash_wl_get_   │
│   bdl()                    │  │   blockdev()               │
└────────────────────────────┘  └────────────────────────────┘
     ↓                                     ↓
┌──────────────────────────────────────────────────────────────┐
│ NAND Wear-Leveling BDL (esp_blockdev_t interface)           │
│ (dhara_glue.c, nand_wl_blockdev.c)                          │
│ - Logical-to-physical mapping                                │
│ - Wear leveling (Dhara integration)                          │
│ - Bad block management                                       │
│ - Garbage collection                                         │
│ - TRIM support                                               │
└──────────────────────────────────────────────────────────────┘
     ↓
┌──────────────────────────────────────────────────────────────┐
│ NAND Flash BDL (esp_blockdev_t interface)                   │
│ (nand_flash_blockdev.c, nand_impl.c)                        │
│ - Physical page/block operations                             │
│ - Device-specific implementations                            │
│ - ECC error handling and statistics                          │
│ - Bad block detection and marking                            │
│ - Direct hardware access                                     │
│ - IOCTL commands for advanced operations                     │
└──────────────────────────────────────────────────────────────┘
     ↓
┌──────────────────────────────────────────────────────────────┐
│ SPI NAND Operations / Emulation                              │
│ (spi_nand_oper.c / nand_linux_mmap_emul.c)                  │
│ - SPI transaction handling (ESP)                             │
│ - Memory-mapped file emulation (Linux)                       │
│ - Command execution                                          │
│ - Register access                                            │
└──────────────────────────────────────────────────────────────┘
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

- **`spi_nand_flash.h`** - Main public API
  - **Legacy API** (Always available):
    - `spi_nand_flash_init_device()` - Initialize NAND flash device
    - `spi_nand_flash_read_sector()` - Read logical sector
    - `spi_nand_flash_write_sector()` - Write logical sector
    - `spi_nand_flash_trim()` - Trim/discard logical sector
    - `spi_nand_flash_sync()` - Synchronize cache to device
    - `spi_nand_flash_gc()` - Explicit garbage collection
    - `spi_nand_flash_copy_sector()` - Copy sector
    - `spi_nand_flash_get_capacity()` - Get number of sectors
    - `spi_nand_flash_get_sector_size()` - Get sector size
    - `spi_nand_flash_get_block_size()` - Get block size
    - `spi_nand_flash_get_block_num()` - Get number of blocks
    - `spi_nand_erase_chip()` - Erase entire chip
    - `spi_nand_flash_deinit_device()` - De-initialize device
  - **BDL API** (Conditional - requires `CONFIG_NAND_FLASH_ENABLE_BDL`):
    - `spi_nand_flash_init_with_layers()` - Initialize with BDL handles

- **`nand_device_types.h`** - Common types and definitions
  - `nand_ecc_status_t` - ECC status enumeration
  - `nand_device_info_t` - Device identification (manufacturer, device ID, chip name)
  - `nand_flash_geometry_t` - Flash geometry (page size, block size, ECC data)
  - `nand_flash_info_t` - Complete flash information

- **`esp_nand_blockdev.h`** - Block device interface (Conditional - requires `CONFIG_NAND_FLASH_ENABLE_BDL`)
  - `nand_flash_get_blockdev()` - Create Flash BDL
  - `spi_nand_flash_wl_get_blockdev()` - Create Wear-Leveling BDL
  - NAND-specific ioctl commands
  - Argument structures for ioctl operations

- **`esp_vfs_fat_nand.h`** - VFS FAT integration
  - **Legacy VFS API** (Always available):
    - `esp_vfs_fat_nand_mount()` - Mount FATFS using legacy handle
    - `esp_vfs_fat_nand_unmount()` - Unmount legacy FATFS
  - **BDL VFS API** (Conditional - requires `CONFIG_NAND_FLASH_ENABLE_BDL`):
    - `esp_vfs_fat_nand_mount_bdl()` - Mount FATFS using BDL handle
    - `esp_vfs_fat_nand_unmount_bdl()` - Unmount BDL FATFS

- **`nand_diag_api.h`** - Diagnostic and statistics API
  - `spi_nand_flash_print_ecc_stat()` - Print ECC statistics

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
├── nand.c                      # Public API implementation (Always compiled)
│                               # - spi_nand_flash_init_device()
│                               # - spi_nand_flash_read_sector()
│                               # - spi_nand_flash_write_sector()
│                               # - spi_nand_flash_trim()
│                               # - spi_nand_flash_sync()
│                               # - spi_nand_flash_gc()
│                               # - spi_nand_flash_init_with_layers() [BDL only]
│
├── nand_impl.c                 # Flash layer implementation (Always compiled)
│                               # - Device detection and initialization
│                               # - nand_read(), nand_prog(), nand_erase()
│                               # - nand_is_bad(), nand_mark_bad()
│                               # - nand_is_free()
│                               # - ECC error detection and handling
│                               # - Plane selection support
│
├── nand_impl_wrap.c            # Wrapper for nand_impl operations (Always compiled)
│                               # - Mutex-protected wrappers
│
├── dhara_glue.c                # Wear-Leveling implementation (Always compiled)
│                               # - Dhara library integration
│                               # - nand_wl_attach_ops() / nand_wl_detach_ops()
│                               # - Logical-to-physical mapping
│                               # - Conditional BDL handle support
│
├── nand_flash_blockdev.c       # Flash BDL adapter [BDL only]
│                               # - nand_flash_get_blockdev()
│                               # - esp_blockdev_t interface implementation
│                               # - IOCTL command handling
│                               # - Boundary checks for operations
│
├── nand_wl_blockdev.c          # WL BDL adapter [BDL only]
│                               # - spi_nand_flash_wl_get_blockdev()
│                               # - esp_blockdev_t interface implementation
│                               # - Wear-leveling operations
│                               # - Sector read/write/trim
│                               # - Function pointer validation
│
├── nand_diag_api.c             # Diagnostic and statistics API
│                               # - ECC statistics
│                               # - Boundary checks
│
├── spi_nand_oper.c             # SPI operations (ESP targets only)
│                               # - SPI transaction handling
│                               # - Multi-mode support (SIO/DIO/QIO)
│
├── nand_linux_mmap_emul.c      # Linux emulation (Linux target only)
│                               # - Memory-mapped file I/O
│
└── devices/                    # Device-specific implementations (ESP targets only)
    ├── nand_winbond.c          # Winbond NAND flash support
    ├── nand_gigadevice.c       # GigaDevice NAND flash support
    ├── nand_alliance.c         # Alliance NAND flash support
    ├── nand_micron.c           # Micron NAND flash support
    ├── nand_zetta.c            # Zetta NAND flash support
    └── nand_xtx.c              # XTX NAND flash support
```

### Diskio Layer (`diskio/`)

```
diskio/
├── diskio_nand.c               # FATFS diskio for legacy API (Always compiled)
│                               # - ff_diskio_register_nand()
│                               # - Sector-based read/write/trim
│
├── diskio_nand.h               # Header for legacy diskio
│
├── diskio_nand_blockdev.c      # FATFS diskio for BDL [BDL only]
│                               # - ff_diskio_register_blockdev()
│                               # - Block device based operations
│
└── diskio_nand_blockdev.h      # Header for BDL diskio
```

### VFS Layer (`vfs/`)

```
vfs/
├── vfs_fat_spinandflash.c      # VFS FAT integration
│                               # - esp_vfs_fat_nand_mount() [Always]
│                               # - esp_vfs_fat_nand_unmount() [Always]
│                               # - esp_vfs_fat_nand_mount_bdl() [BDL only]
│                               # - esp_vfs_fat_nand_unmount_bdl() [BDL only]
│
└── esp_vfs_fat_nand.h          # VFS FAT header
```

## API Usage

### Legacy API (Always Available)

The legacy API provides a simple interface for reading and writing logical sectors with automatic wear-leveling. This API is always available regardless of Kconfig settings.

```c
#include "spi_nand_flash.h"
#include "esp_vfs_fat_nand.h"

// Configure SPI NAND flash
spi_nand_flash_config_t config = {
    .device_handle = spi_handle,
    .io_mode = SPI_NAND_IO_MODE_QIO,
    .flags = SPI_DEVICE_HALFDUPLEX,
};

spi_nand_flash_device_t *handle;

// Initialize device
esp_err_t ret = spi_nand_flash_init_device(&config, &handle);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize NAND flash");
    return ret;
}

// Read/write sectors
uint8_t buffer[2048];
uint32_t sector_id = 0;

ret = spi_nand_flash_read_sector(handle, buffer, sector_id);
ret = spi_nand_flash_write_sector(handle, buffer, sector_id);

// TRIM (mark sector as free for garbage collection)
ret = spi_nand_flash_trim(handle, sector_id);

// Explicit garbage collection (optional - happens automatically)
ret = spi_nand_flash_gc(handle);

// Synchronize cache to device
ret = spi_nand_flash_sync(handle);

// Get capacity information
uint32_t num_sectors, sector_size;
spi_nand_flash_get_capacity(handle, &num_sectors);
spi_nand_flash_get_sector_size(handle, &sector_size);
ESP_LOGI(TAG, "Capacity: %u sectors of %u bytes", num_sectors, sector_size);

// Mount FATFS (legacy method)
esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = true,
    .allocation_unit_size = 16 * 1024
};
ret = esp_vfs_fat_nand_mount("/nand", handle, &mount_config);

// Use filesystem...
FILE *f = fopen("/nand/test.txt", "w");
fprintf(f, "Hello NAND!\n");
fclose(f);

// Unmount and cleanup
esp_vfs_fat_nand_unmount("/nand", handle);
spi_nand_flash_deinit_device(handle);
```

### BDL API (Requires CONFIG_NAND_FLASH_ENABLE_BDL=y)

The BDL API provides direct access to block device layers, enabling advanced features like raw flash access, detailed ECC statistics, and custom filesystem integration.

#### Method 1: Direct Layer Creation

```c
#include "spi_nand_flash.h"
#include "esp_nand_blockdev.h"
#include "esp_vfs_fat_nand.h"

// Configure SPI NAND flash
spi_nand_flash_config_t config = {
    .device_handle = spi_handle,
    .io_mode = SPI_NAND_IO_MODE_QIO,
    .flags = SPI_DEVICE_HALFDUPLEX,
};

esp_blockdev_handle_t flash_bdl;
esp_blockdev_handle_t wl_bdl;

// Step 1: Create Flash Block Device Layer (raw NAND access)
esp_err_t ret = nand_flash_get_blockdev(&config, &flash_bdl);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create Flash BDL");
    return ret;
}

// Optional: Access raw flash operations directly
uint8_t page_buffer[2048];
uint32_t page_addr = 0;
flash_bdl->ops->read(flash_bdl, page_buffer, sizeof(page_buffer), page_addr, sizeof(page_buffer));

// Check if a block is bad
esp_blockdev_cmd_arg_status_t bad_block_cmd = { .num = 10 };
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_block_cmd);
if (bad_block_cmd.status) {
    ESP_LOGW(TAG, "Block 10 is marked as bad");
}

// Get detailed flash information
nand_flash_info_t flash_info;
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO, &flash_info);
ESP_LOGI(TAG, "Chip: %s, Manufacturer: 0x%02x, Device: 0x%02x", 
         flash_info.device_info.chip_name,
         flash_info.device_info.mfr_id,
         flash_info.device_info.dev_id);
ESP_LOGI(TAG, "Geometry: %u blocks, %u pages/block, %u bytes/page",
         flash_info.geometry.num_blocks,
         flash_info.geometry.pages_per_block,
         flash_info.geometry.page_size);

// Get ECC statistics
esp_blockdev_cmd_arg_ecc_stats_t ecc_stats;
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_GET_ECC_STATS, &ecc_stats);
ESP_LOGI(TAG, "ECC: %u errors, %u exceeding threshold, %u uncorrected",
         ecc_stats.ecc_err_total_count,
         ecc_stats.ecc_err_exceeding_threshold_count,
         ecc_stats.ecc_err_not_corrected_count);

// Step 2: Create Wear-Leveling Block Device Layer
ret = spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create WL BDL");
    flash_bdl->ops->release(flash_bdl);
    return ret;
}

// Use wear-leveling layer for sector-based operations
uint8_t sector_buffer[2048];
uint32_t sector_id = 0;
wl_bdl->ops->read(wl_bdl, sector_buffer, sizeof(sector_buffer), sector_id, sizeof(sector_buffer));
wl_bdl->ops->write(wl_bdl, sector_buffer, sector_id, sizeof(sector_buffer));

// Get available sectors
uint32_t avail_sectors;
wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &avail_sectors);
ESP_LOGI(TAG, "Available sectors: %u", avail_sectors);

// Cleanup (releases both WL and Flash layers)
wl_bdl->ops->release(wl_bdl);
```

#### Method 2: Simplified Initialization with Filesystem

```c
#include "spi_nand_flash.h"
#include "esp_nand_blockdev.h"
#include "esp_vfs_fat_nand.h"

// Configure SPI NAND flash
spi_nand_flash_config_t config = {
    .device_handle = spi_handle,
    .io_mode = SPI_NAND_IO_MODE_QIO,
    .flags = SPI_DEVICE_HALFDUPLEX,
};

esp_blockdev_handle_t wl_bdl;

// Initialize both layers at once (simplified)
esp_err_t ret = spi_nand_flash_init_with_layers(&config, &wl_bdl);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize NAND flash with layers");
    return ret;
}

// Mount FATFS using BDL handle
esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = true,
    .allocation_unit_size = 16 * 1024
};

ret = esp_vfs_fat_nand_mount_bdl("/nand", wl_bdl, &mount_config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount FATFS");
    wl_bdl->ops->release(wl_bdl);
    return ret;
}

// Use filesystem...
FILE *f = fopen("/nand/test.txt", "w");
fprintf(f, "Hello from BDL!\n");
fclose(f);

// Unmount and cleanup
esp_vfs_fat_nand_unmount_bdl("/nand", wl_bdl);
```

## Block Device IOCTL Commands

IOCTL commands provide advanced operations and diagnostics for block devices. These commands are only available when using the BDL API (`CONFIG_NAND_FLASH_ENABLE_BDL=y`).

### Flash BDL Commands

These commands operate on the Flash Block Device Layer for low-level hardware access:

| Command | Description | Argument Type | Example |
|---------|-------------|---------------|---------|
| `ESP_BLOCKDEV_CMD_IS_BAD_BLOCK` | Check if a physical block is marked bad | `esp_blockdev_cmd_arg_status_t*` | Check block health before raw access |
| `ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK` | Mark a physical block as bad | `uint32_t*` (block number) | Mark block after repeated failures |
| `ESP_BLOCKDEV_CMD_IS_FREE_PAGE` | Check if a page is erased (0xFF) | `esp_blockdev_cmd_arg_status_t*` | Verify erase operation |
| `ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS` | Get ECC correction status for a page | `esp_blockdev_cmd_arg_ecc_status_t*` | Monitor bit error rates |
| `ESP_BLOCKDEV_CMD_GET_BAD_BLOCK_COUNT` | Get total count of bad blocks | `uint32_t*` | Flash health monitoring |
| `ESP_BLOCKDEV_CMD_GET_ECC_STATS` | Get comprehensive ECC statistics | `esp_blockdev_cmd_arg_ecc_stats_t*` | Detect flash degradation |
| `ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO` | Get complete device info and geometry | `nand_flash_info_t*` | Query chip details |

### WL BDL Commands

These commands operate on the Wear-Leveling Block Device Layer for logical sector management:

| Command | Description | Argument Type | Example |
|---------|-------------|---------------|---------|
| `ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS` | Get number of usable logical sectors | `uint32_t*` | Check filesystem capacity |
| `ESP_BLOCKDEV_CMD_TRIM_SECTOR` | Mark logical sector as unused (TRIM) | `uint32_t*` (sector number) | Optimize after file deletion |

### Example: Using IOCTL Commands

```c
esp_blockdev_handle_t flash_bdl;
nand_flash_get_blockdev(&config, &flash_bdl);

// Check if block 10 is bad
esp_blockdev_cmd_arg_status_t status_cmd = { .num = 10 };
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &status_cmd);
if (status_cmd.status) {
    ESP_LOGW(TAG, "Block 10 is bad");
}

// Get ECC statistics
esp_blockdev_cmd_arg_ecc_stats_t ecc_stats;
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_GET_ECC_STATS, &ecc_stats);
ESP_LOGI(TAG, "Total ECC errors: %u", ecc_stats.ecc_err_total_count);
ESP_LOGI(TAG, "ECC errors exceeding threshold: %u", ecc_stats.ecc_err_exceeding_threshold_count);
ESP_LOGI(TAG, "Uncorrectable ECC errors: %u", ecc_stats.ecc_err_not_corrected_count);

// Get flash information
nand_flash_info_t info;
flash_bdl->ops->ioctl(flash_bdl, ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO, &info);
ESP_LOGI(TAG, "Chip: %s", info.device_info.chip_name);
ESP_LOGI(TAG, "Capacity: %u blocks × %u pages × %u bytes",
         info.geometry.num_blocks,
         info.geometry.pages_per_block,
         info.geometry.page_size);
```

## Examples

The component provides three example projects:

### 1. `examples/nand_flash/` - Legacy API Example

Demonstrates the basic legacy API usage:
- Initializes SPI NAND flash device
- Mounts FATFS using `esp_vfs_fat_nand_mount()`
- Performs file operations
- No BDL dependencies

**Build and run:**
```bash
cd examples/nand_flash
idf.py build flash monitor
```

### 2. `examples/nand_flash_bdl/` - BDL API Example

Demonstrates the Block Device Layer API (requires `CONFIG_NAND_FLASH_ENABLE_BDL=y`):
- Uses `spi_nand_flash_init_with_layers()` to get BDL handles
- Mounts FATFS using `esp_vfs_fat_nand_mount_bdl()`
- Shows how to use block device operations
- Demonstrates BDL-based filesystem integration

**Build and run:**
```bash
cd examples/nand_flash_bdl
idf.py build flash monitor
```

### 3. `examples/nand_flash_debug_app/` - Diagnostic Tool

Comprehensive diagnostic application for testing and debugging:
- Tests all API functions
- Prints detailed flash information
- Runs ECC statistics
- Useful for validating new hardware

## Testing and Validation

### Target Testing

The component includes a comprehensive test application in `test_app/`:

```bash
cd test_app
idf.py build flash monitor
```

Tests are automatically selected based on Kconfig:
- **Legacy tests** (`test_spi_nand_flash.c`): Run when `CONFIG_NAND_FLASH_ENABLE_BDL=n`
- **BDL tests** (`test_spi_nand_flash_bdl.c`): Run when `CONFIG_NAND_FLASH_ENABLE_BDL=y`

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
    .emul_conf = &emul_cfg,
    // ... other config ...
};

// Use normally - will emulate NAND flash
spi_nand_flash_device_t *handle;
spi_nand_flash_init_device(&config, &handle);
#endif
```

**Build and run host tests:**
```bash
cd host_test
idf.py --preview set-target linux
idf.py build monitor
```

Host tests also conditionally compile based on Kconfig:
- **Legacy tests** (`test_nand_flash.cpp`): Compiled when `CONFIG_NAND_FLASH_ENABLE_BDL=n`
- **BDL tests** (`test_nand_flash_bdl.cpp`): Compiled when `CONFIG_NAND_FLASH_ENABLE_BDL=y`

See `host_test/README.md` for more details.

## Safety Improvements

The component includes comprehensive safety checks to prevent common programming errors:

### Boundary Checks

All division and modulo operations include zero-divisor checks:

```c
// Example from nand_wl_blockdev.c
if (sector_size == 0) {
    ESP_LOGE(TAG, "Invalid sector size (0)");
    return ESP_ERR_INVALID_SIZE;
}
uint32_t sector_count = erase_len / sector_size;
```

**Protected operations:**
- Sector size calculations
- Block size calculations
- Page count calculations
- Plane selection (modulo operations)

### Alignment Validation

Read, write, and erase operations validate address and length alignment:

```c
// Example: Erase alignment check
if ((start_addr % sector_size) != 0) {
    ESP_LOGE(TAG, "Start address not aligned to sector size");
    return ESP_ERR_INVALID_ARG;
}

if ((erase_len % sector_size) != 0) {
    ESP_LOGE(TAG, "Erase length not aligned to sector size");
    return ESP_ERR_INVALID_ARG;
}
```

### Function Pointer Validation

BDL creation validates all required operations are available:

```c
// Example from spi_nand_flash_wl_get_blockdev()
ESP_RETURN_ON_FALSE(nand_bdl != NULL, ESP_ERR_INVALID_ARG, TAG, "nand_bdl cannot be NULL");
ESP_RETURN_ON_FALSE(nand_bdl->ops != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL ops cannot be NULL");
ESP_RETURN_ON_FALSE(nand_bdl->ops->read != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL read operation is required");
ESP_RETURN_ON_FALSE(nand_bdl->ops->write != NULL, ESP_ERR_INVALID_STATE, TAG, "Flash BDL write operation is required");
// ... more validation
```

## Migration Guide

### For Existing Projects

**No changes required!** The existing API is fully supported and will continue to work. The implementation now uses the layered architecture internally, providing:
- Better reliability with comprehensive safety checks
- Enhanced error handling and reporting
- Improved maintainability

### For New Projects


#### Use BDL API to get:
- Direct flash access (raw page/block operations)
- Advanced diagnostics operations (ECC stats, bad block tracking)
- Custom filesystem integration is desired
- Fine-grained control over layers is needed

### Enabling BDL Support

To enable BDL support in your project:

1. **Via menuconfig:**
   ```
   idf.py menuconfig
   → Component config
   → SPI NAND Flash configuration
   → [*] Enable Block Device Layer (BDL) support
   ```

## Error Handling

The layered architecture provides comprehensive error reporting at each level:

### Hardware Layer Errors
- **SPI Communication Failures**: `ESP_ERR_TIMEOUT`, `ESP_FAIL`
- **ECC Errors**: `ESP_ERR_FLASH_BASE + DHARA_E_ECC`
- **Bad Block Detection**: Automatic detection and remapping

### Flash Layer Errors
- **Bad Block**: `ESP_ERR_NOT_FINISHED` when programming to bad block
- **ECC Failure**: `ESP_ERR_FLASH_BASE + DHARA_E_ECC` for uncorrectable errors
- **Invalid Parameters**: `ESP_ERR_INVALID_ARG`, `ESP_ERR_INVALID_SIZE`
- **Alignment Errors**: `ESP_ERR_INVALID_ARG` for misaligned addresses

### Wear-Leveling Layer Errors
- **Out of Space**: `ESP_ERR_FLASH_BASE + DHARA_E_NAND_FAILED`
- **Mapping Errors**: `ESP_ERR_FLASH_BASE + DHARA_E_MAP_FAILED`
- **Garbage Collection Issues**: Automatically retried or reported
- **Too Many Bad Blocks**: `ESP_ERR_NO_MEM` when insufficient spare blocks


## Build Configuration

### CMakeLists.txt Integration

The component's `CMakeLists.txt` conditionally compiles BDL-related files:

```cmake
# Base sources (always compiled)
set(srcs "src/nand.c"
         "src/dhara_glue.c"
         "src/nand_impl_wrap.c"
         "diskio/diskio_nand.c")

# Conditionally add BDL support
if(CONFIG_NAND_FLASH_ENABLE_BDL)
    list(APPEND reqs esp_blockdev)
    list(APPEND srcs "src/nand_flash_blockdev.c"
                     "src/nand_wl_blockdev.c"
                     "diskio/diskio_nand_blockdev.c")
endif()
```

## Supported Manufacturers

The component supports NAND flash chips from multiple manufacturers:

| Manufacturer | File | Example Chips |
|--------------|------|---------------|
| Winbond | `src/devices/nand_winbond.c` | W25N01GV, W25N02KV |
| GigaDevice | `src/devices/nand_gigadevice.c` | GD5F1GQ5UExxG |
| Alliance | `src/devices/nand_alliance.c` | AS5F31G04SND |
| Micron | `src/devices/nand_micron.c` | MT29F1G01 |
| Zetta | `src/devices/nand_zetta.c` | ZD35Q1GA |
| XTX | `src/devices/nand_xtx.c` | XT26G01C |

Device detection is automatic based on JEDEC manufacturer and device IDs.

## Summary

This layered architecture provides:

**Backward Compatibility**: Existing code works without changes  
**Conditional Compilation**: Enable BDL only when needed  
**Safety**: Comprehensive boundary checks and validation  
**Flexibility**: Choose between simple legacy API or advanced BDL API  
**Testability**: Linux host testing support  
**Extensibility**: Easy to add new features and devices  
**Maintainability**: Clear separation of concerns  

The modular design makes the component production-ready while remaining extensible for future development.
