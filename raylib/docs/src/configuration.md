# Configuration Options

Run `idf.py menuconfig` and open **Component config → Raylib**.

## Framebuffer and memory

| Option | Default | Effect |
| --- | --- | --- |
| `CONFIG_RAYLIB_FRAMEBUFFER_FORMAT` | RGB565 | Pixel format of the framebuffer and of the pixels your callback receives. 16 bpp is what panels take natively; RGBA8888 is raylib's own format and needs no conversion. |
| `CONFIG_RAYLIB_USE_PSRAM` | on when PSRAM is available | Place large buffers (framebuffer, textures) in PSRAM instead of internal RAM; smaller ones stay internal. |

### Pixel formats

| Format | Bytes per pixel | Layout |
| --- | --- | --- |
| RGB565 | 2 | R in bits 15:11, G in 10:5, B in 4:0 |
| RGB888 | 3 | R, then G, then B |
| RGBA8888 | 4 | R, then G, then B, then A |
| RGB332 | 1 | R in bits 7:5, G in 4:2, B in 1:0 |

`CONFIG_RAYLIB_USE_PSRAM` is visible only when `CONFIG_SPIRAM` is enabled. Blocks of 32 KiB and above go to PSRAM first and fall back to internal RAM; smaller blocks, which the renderer walks constantly, stay in internal RAM.

The pixel format must agree with what your panel expects. If colors look striped or shifted, check this option and the [swap_bytes](api.md#struct-esp_raylib_display_config_t) flag first.

## Resource loaders

Loaders are independently combinable and all off by default — with all off, assets come from memory, for example data embedded in the binary.

| Option | Effect |
| --- | --- |
| `CONFIG_RAYLIB_FILEFORMAT_QOI` | QOI image — raylib's own format and the cheapest. |
| `CONFIG_RAYLIB_FILEFORMAT_PNG` | PNG image — raylib carries its own decoder, so this does not need zlib. Worth a couple of kilobytes of flash. |
| `CONFIG_RAYLIB_FILEFORMAT_BMP` | BMP image — uncompressed pixels: tiny decoder, larger files. |
| `CONFIG_RAYLIB_FILEFORMAT_TGA` | TGA image — uncompressed via stb_image; a format art tools export without options to get wrong. |
| `CONFIG_RAYLIB_FILEFORMAT_JPG` | JPG image — via stb_image, worth a few kilobytes: the format for photographic assets. |
| `CONFIG_RAYLIB_FILEFORMAT_GIF` | GIF image — palette-based, at most 8 bpp, and the only loader here that also reads animations (`LoadImageAnim`). |
| `CONFIG_RAYLIB_FILEFORMAT_TTF` | TTF font — `LoadFontEx()`, rasterised into a texture atlas at load time through stb_truetype. |
| `CONFIG_RAYLIB_FILEFORMAT_FNT` | BMFont (`.fnt`) — `LoadFont()`, glyphs pre-rasterised in an atlas image. Needs an image loader (usually PNG) enabled alongside. |
