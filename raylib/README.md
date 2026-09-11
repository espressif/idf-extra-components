# Raylib for ESP-IDF

ESP-IDF component wrapper for [raylib](https://www.raylib.com/) using the **software renderer** (rlgl OpenGL 1.1 software backend).

This component enables running raylib on ESP32 devices without GPU/OpenGL support by using the CPU-based software renderer merged in [raylib PR #4832](https://github.com/raysan5/raylib/pull/4832).

**[Component Registry](https://components.espressif.com/components/espressif/raylib)** - Available via ESP Component Manager (`idf.py add-component`)

## Architecture

This implementation uses a **direct BSP integration** approach where board-specific examples handle display initialization and register callbacks with raylib:

```
Application (main.c)
    ↓
Display Init (BSP or direct esp_lcd)
    ↓
rcore Callbacks (raylib_esp_set_display_callbacks)
    ↓
raylib (rcore_esp_idf.c platform backend)
    ↓
esp_lcd (ESP-IDF display driver)
```

**Key benefits:**
- Simple callback interface for display integration
- Template-based examples for consistency
- BSP support for common boards
- Direct esp_lcd init for boards without BSP
- Supports both SPI and DSI panels

## Features

- Software rendering (no OpenGL hardware required)
- RGB565 framebuffer support for ESP LCD panels
- PSRAM allocation for framebuffers
- Automatic byte swapping for SPI panels
- LCD synchronization with semaphores
- Dynamic display dimensions via callbacks
- Chunked transfers for SPI panels
- Compatible with esp-bsp noglib components
- 2D graphics: shapes, textures, text rendering

## Supported Boards

**Fully tested and working:**

| Board | Display | Chip | Init Type |
|-------|---------|------|-----------|
| ESP32-S3-BOX-3 | 320x240 ILI9341 | ESP32-S3 | BSP |
| ESP32-S3-LCD-EV | 480x480 GC9503 | ESP32-S3 | BSP |
| ESP32-S3-Korvo-2 | 320x240 ILI9341 | ESP32-S3 | BSP |
| ESP32-S3-BOX | 320x240 ST7789 | ESP32-S3 | Direct |
| ESP-VoCat | 360x360 ST77916 | ESP32-S3 | BSP |
| ESP32-S3-EYE | 240x240 ST7789 | ESP32-S3 | BSP |
| M5Stack CoreS3 | 320x240 ILI9341 | ESP32-S3 | BSP |
| M5Stack AtomS3 | 128x128 GC9A01 | ESP32-S3 | BSP |
| M5Stack AtomS3R | 128x128 GC9107 | ESP32-S3 | Direct |
| M5Stack Core2 | 320x240 ILI9342 | ESP32 | BSP |

## Requirements

- ESP-IDF 5.5 or later (CI tests with v6.1)
- ESP32-S3 or ESP32 with PSRAM (recommended)
- Board Support Package (BSP) for supported boards

## Quick Start

Board-specific examples are organized by chip:

**ESP32-S3 boards:**
```bash
cd raylib/examples/esp32s3/espressif-esp32-s3-box-3_hello
idf.py set-target esp32s3
idf.py build flash monitor
```

**ESP32 boards:**
```bash
cd raylib/examples/esp32/m5stack-core2_hello
idf.py set-target esp32
idf.py build flash monitor
```

**Available examples:**
```
raylib/examples/
├── esp32/
│   └── m5stack-core2_hello/
└── esp32s3/
    ├── espressif-esp32-s3-box-3_hello/
    ├── espressif-esp32-s3-box_hello/
    ├── espressif-esp32-s3-eye_hello/
    ├── espressif-esp32-s3-korvo-2_hello/
    ├── espressif-esp32-s3-lcd-ev-board_hello/
    ├── espressif-esp-vocat_hello/
    ├── m5stack-atom-s3_hello/
    ├── m5stack-atom-s3r_hello/
    └── m5stack-core-s3_hello/
```

## Component Structure

```
raylib/
├── CMakeLists.txt              # ESP-IDF component build configuration
├── idf_component.yml           # Component metadata
├── include/                    # Wrapper headers
│   ├── dirent.h               # Stub for missing POSIX functions
│   └── rlsw_esp_idf.h         # Software renderer config
├── src/
│   └── platforms/
│       └── rcore_esp_idf.c    # ESP-IDF platform backend
├── raylib/                     # Git submodule: official raylib
├── templates/                  # Example templates
│   └── raylib-hello-c/         # Hello example template
├── scripts/                    # Utility scripts
│   └── regenerate-all.sh      # Regenerate examples from template
└── examples/                   # Generated board examples
    ├── esp32/                  # ESP32 chip examples
    │   └── m5stack-core2_hello/
    └── esp32s3/                # ESP32-S3 chip examples
        ├── espressif-esp32-s3-box-3_hello/
        ├── m5stack-core-s3_hello/
        └── ...
```

## Configuration

The component is configured via CMake definitions in `CMakeLists.txt`:

- `GRAPHICS_API_OPENGL_SOFTWARE` - Enable software renderer
- `SW_COLOR_BUFFER_BITS=16` - RGB565 format
- `SW_FRAMEBUFFER_COLOR_TYPE=R5G6B5` - RGB565 pixel format
- Disabled modules: raudio, rmodels, compression, automation

## Display Integration

### BSP Integration (For Boards with BSP)

Use the Board Support Package for simple display initialization:

```c
// Display Initialization
bsp_display_config_t cfg = {
    .max_transfer_sz = 320 * 48 * sizeof(uint16_t),
};
bsp_display_new(&cfg, &panel, &io);
esp_lcd_panel_disp_on_off(panel, true);
bsp_display_backlight_on();

// Register callbacks with raylib
raylib_esp_set_display_callbacks(display_flush, display_get_dimensions);
```

### Direct esp_lcd Integration (For Boards without BSP)

For boards without BSP support, initialize display directly:

```c
// SPI bus configuration
spi_bus_config_t bus_cfg = {
    .sclk_io_num = GPIO_NUM_15,
    .mosi_io_num = GPIO_NUM_21,
    .miso_io_num = GPIO_NUM_NC,
    .max_transfer_sz = 128 * 128 * sizeof(uint16_t),
};
spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

// Panel IO configuration
esp_lcd_panel_io_spi_config_t io_cfg = {
    .cs_gpio_num = GPIO_NUM_14,
    .dc_gpio_num = GPIO_NUM_42,
    .spi_mode = 0,
    .pclk_hz = 40 * 1000 * 1000,
};
esp_lcd_new_panel_io_spi(SPI3_HOST, &io_cfg, &io);

// Panel configuration
esp_lcd_panel_dev_config_t panel_cfg = {
    .reset_gpio_num = GPIO_NUM_48,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    .bits_per_pixel = 16,
};
esp_lcd_new_panel_gc9107(io, &panel_cfg, &panel);
esp_lcd_panel_init(panel);
esp_lcd_panel_disp_on_off(panel, true);

// Register callbacks
raylib_esp_set_display_callbacks(display_flush, display_get_dimensions);
```

### Callback Interface

The `raylib_esp_set_display_callbacks` function registers two callbacks:

**flush_fn**: Sends framebuffer to display
```c
void display_flush(const uint16_t *buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    // Send buf to display at (x, y) with size (w, h)
    // Chunk into 48-line transfers for SPI panels
    for (uint16_t row = 0; row < h; row += CHUNK_LINES) {
        uint16_t chunk_height = (row + CHUNK_LINES > h) ? (h - row) : CHUNK_LINES;
        esp_lcd_panel_draw_bitmap(panel, x, y + row, x + w, y + row + chunk_height, buf + (row * w));
    }
}
```

**get_dim_fn**: Returns display dimensions
```c
void display_get_dimensions(uint16_t *w, uint16_t *h)
{
    if (w) *w = 320;
    if (h) *h = 240;
}
```

### Important Constants

- **max_transfer_sz**: `320 * 48 * sizeof(uint16_t)` for SPI panels (48-line chunks)
- **CHUNK_LINES**: 48 - Max lines per SPI transfer (BSP limitation)
- **Byte swapping**: Required for SPI panels (little-endian to big-endian)

### Color Format

The framebuffer uses RGB565 format. For SPI panels, bytes must be swapped:
```c
framebuffer[i] = __builtin_bswap16(pixel);  // Little-endian to big-endian
```

## Template System

Board examples are generated from a template using esp-generate. This ensures consistent code structure across all boards.

**Template location:** `raylib/templates/raylib-hello-c/`

**Note:** Template-based generation is used during development. Users can directly use pre-generated examples without needing to regenerate.

### Template Structure

```
templates/raylib-hello-c/
├── template.yaml          # Board options and metadata (read by esp-generate)
├── CMakeLists.txt         # Project build configuration with conditionals
├── sdkconfig.defaults     # ESP-IDF configuration with target chip conditionals
├── wokwi.toml             # Wokwi simulator configuration
├── diagram.json           # Wokwi circuit diagram
├── README.md              # Example documentation
└── main/
    ├── idf_component.yml  # Component dependencies (BSP components)
    ├── main.c             # Main application with board-specific conditionals
    └── README.md          # Component documentation
```

**Purpose of each file:**

- **template.yaml**: Defines available board options, display names, and supported chips. Used by esp-generate to present choices and filter compatible options.

- **CMakeLists.txt**: Build configuration with `idf_build_set_property(MINIMAL_BUILD ON)` to trim unused components. Project name defaults to `esp32s3_hello` but gets renamed during generation.

- **sdkconfig.defaults**: ESP-IDF defaults including `CONFIG_IDF_TARGET` and PSRAM settings. Uses conditionals to set correct target chip per board.

- **wokwi.toml**: Wokwi simulator configuration pointing to build outputs. Uses conditionals to set correct firmware/elf paths per target chip.

- **diagram.json**: Wokwi circuit diagram defining the board, display, and connections.

- **main/idf_component.yml**: Component dependencies including BSP components for supported boards.

- **main/main.c**: Application code with conditional compilation for each board's display initialization, dimensions, and BSP/direct init paths.

### Conditional Compilation Syntax

esp-generate supports these directives:
```c
//IF option("board_name")
    // Code for this board only
//ELIF option("other_board")
    // Code for other board
//ELSE
    // Default code for all other boards
//ENDIF
```

**Note:** No OR (`||`) or AND (`&&`) expressions are supported. Use separate ELIF blocks instead.

## CI/CD

This project includes GitHub Actions workflows for automated building and testing:

**Workflow:** `.github/workflows/build-and-test-examples.yml`

- **Manual trigger** with configurable ESP-IDF version (default: v6.1)
- **Builds all 10 examples** in parallel using matrix strategy
- **Uploads flash files** as artifacts (ZIP with bootloader, partition-table, app binary, flash_args)
- **Wokwi tests** for ESP32-S3-BOX-3 and M5Stack CoreS3 (screenshot capture)

**Running the workflow:**
```bash
# Default (ESP-IDF v6.1)
gh workflow run build-and-test-examples.yml

# Custom ESP-IDF version
gh workflow run build-and-test-examples.yml -f esp_idf_version=release-v5.2
```

**Artifacts produced:**
- `flash-files-{chip}-{board}_hello.zip` - All binaries for flashing
- `flash-files-{chip}-{board}_hello-screenshot.png` - Wokwi screenshot
- `flash-files-{chip}-{board}_hello-logs.txt` - Wokwi serial output

## Known Issues / TODO

- Touch input API ready but not connected to raylib
- Audio module disabled (no esp-idf audio backend)
- 3D models disabled (requires filesystem APIs)
- Performance: ~15-20 FPS on 320x240, slower on 1024x600
- ESP32 chip target: esp-generate bug prevents generation (workaround: manual copy)

## Contributing

Contributions welcome! Especially:
- Additional board support
- Performance optimizations
- Input handling (touch, buttons)

## License

This wrapper component: zlib/libpng (matching raylib)

Raylib: zlib/libpng - see [raylib/raylib/LICENSE](raylib/raylib/LICENSE)

## Credits

- [raylib](https://www.raylib.com/) by Ramon Santamaria (@raysan5)
- Software renderer (rlsw) by Le Juez Victor (@Bigfoot71)
- ESP-IDF platform integration by this component
