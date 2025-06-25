# API Reference

## Header files

- [include/led_strip.h](#file-includeled_striph)
- [include/led_strip_parlio.h](#file-includeled_strip_parlioh)
- [include/led_strip_rmt.h](#file-includeled_strip_rmth)
- [include/led_strip_spi.h](#file-includeled_strip_spih)
- [include/led_strip_types.h](#file-includeled_strip_typesh)
- [interface/led_strip_interface.h](#file-interfaceled_strip_interfaceh)

## File include/led_strip.h

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**led\_strip\_clear**](#function-led_strip_clear) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip) <br>_Clear LED strip (turn off all LEDs)_ |
|  esp\_err\_t | [**led\_strip\_del**](#function-led_strip_del) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip) <br>_Free LED strip resources._ |
|  esp\_err\_t | [**led\_strip\_group\_del**](#function-led_strip_group_del) ([**led\_strip\_group\_handle\_t**](#typedef-led_strip_group_handle_t) group) <br>_Delete the LED strip group._ |
|  esp\_err\_t | [**led\_strip\_group\_get\_strip\_handle**](#function-led_strip_group_get_strip_handle) ([**led\_strip\_group\_handle\_t**](#typedef-led_strip_group_handle_t) group, uint8\_t index, [**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) \*ret\_strip) <br>_Get the handle of the LED strip._ |
|  esp\_err\_t | [**led\_strip\_refresh**](#function-led_strip_refresh) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip) <br>_Refresh memory colors to LEDs._ |
|  esp\_err\_t | [**led\_strip\_refresh\_async**](#function-led_strip_refresh_async) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip) <br>_Refresh memory colors to LEDs asynchronously._ |
|  esp\_err\_t | [**led\_strip\_refresh\_wait\_async\_done**](#function-led_strip_refresh_wait_async_done) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip) <br>_Wait for the async refresh to complete._ |
|  esp\_err\_t | [**led\_strip\_set\_pixel**](#function-led_strip_set_pixel) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip, uint32\_t index, uint32\_t red, uint32\_t green, uint32\_t blue) <br>_Set RGB for a specific pixel._ |
|  esp\_err\_t | [**led\_strip\_set\_pixel\_hsv**](#function-led_strip_set_pixel_hsv) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip, uint32\_t index, uint16\_t hue, uint8\_t saturation, uint8\_t value) <br>_Set HSV for a specific pixel._ |
|  esp\_err\_t | [**led\_strip\_set\_pixel\_rgbw**](#function-led_strip_set_pixel_rgbw) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip, uint32\_t index, uint32\_t red, uint32\_t green, uint32\_t blue, uint32\_t white) <br>_Set RGBW for a specific pixel._ |
|  esp\_err\_t | [**led\_strip\_switch\_gpio**](#function-led_strip_switch_gpio) ([**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) strip, gpio\_num\_t new\_gpio\_num, bool invert\_output) <br>_Switch GPIO of LED strip._ |

## Functions Documentation

### function `led_strip_clear`

_Clear LED strip (turn off all LEDs)_

```c
esp_err_t led_strip_clear (
    led_strip_handle_t strip
)
```

**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Clear LEDs successfully
- ESP\_FAIL: Clear LEDs failed because some other error occurred

### function `led_strip_del`

_Free LED strip resources._

```c
esp_err_t led_strip_del (
    led_strip_handle_t strip
)
```

**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Free resources successfully
- ESP\_FAIL: Free resources failed because error occurred

### function `led_strip_group_del`

_Delete the LED strip group._

```c
esp_err_t led_strip_group_del (
    led_strip_group_handle_t group
) 
```

**Parameters:**

- `group` Handle of the LED strip group

**Returns:**

- ESP\_OK: Delete the LED strip group successfully
- ESP\_ERR\_INVALID\_ARG: Invalid argument

### function `led_strip_group_get_strip_handle`

_Get the handle of the LED strip._

```c
esp_err_t led_strip_group_get_strip_handle (
    led_strip_group_handle_t group,
    uint8_t index,
    led_strip_handle_t *ret_strip
) 
```

**Parameters:**

- `group` LED strip group handle
- `index` Index of the LED strip in the group
- `ret_strip` Pointer to store the handle of the LED strip

**Returns:**

- ESP\_OK: Get the handle of the LED strip successfully
- ESP\_ERR\_INVALID\_ARG: Invalid argument

### function `led_strip_refresh`

_Refresh memory colors to LEDs._

```c
esp_err_t led_strip_refresh (
    led_strip_handle_t strip
)
```

**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Refresh successfully
- ESP\_FAIL: Refresh failed because some other error occurred

**Note:**

After updating the LED colors in the memory, a following invocation of this API is needed to flush colors to strip.

### function `led_strip_refresh_async`

_Refresh memory colors to LEDs asynchronously._

```c
esp_err_t led_strip_refresh_async (
    led_strip_handle_t strip
) 
```

**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Refresh successfully
- ESP\_FAIL: Refresh failed because some other error occurred

**Note:**

This function is non-blocking, so you need to call `led_strip_refresh_wait_async_done` to wait for the refresh to complete before modifying the LED colors again.

### function `led_strip_refresh_wait_async_done`

_Wait for the async refresh to complete._

```c
esp_err_t led_strip_refresh_wait_async_done (
    led_strip_handle_t strip
) 
```

**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Wait for the async refresh to complete successfully

### function `led_strip_set_pixel`

_Set RGB for a specific pixel._

```c
esp_err_t led_strip_set_pixel (
    led_strip_handle_t strip,
    uint32_t index,
    uint32_t red,
    uint32_t green,
    uint32_t blue
)
```

**Parameters:**

- `strip` LED strip
- `index` index of pixel to set
- `red` red part of color
- `green` green part of color
- `blue` blue part of color

**Returns:**

- ESP\_OK: Set RGB for a specific pixel successfully
- ESP\_ERR\_INVALID\_ARG: Set RGB for a specific pixel failed because of invalid parameters
- ESP\_FAIL: Set RGB for a specific pixel failed because other error occurred

### function `led_strip_set_pixel_hsv`

_Set HSV for a specific pixel._

```c
esp_err_t led_strip_set_pixel_hsv (
    led_strip_handle_t strip,
    uint32_t index,
    uint16_t hue,
    uint8_t saturation,
    uint8_t value
)
```

**Parameters:**

- `strip` LED strip
- `index` index of pixel to set
- `hue` hue part of color (0 - 360)
- `saturation` saturation part of color (0 - 255, rescaled from 0 - 1. e.g. saturation = 0.5, rescaled to 127)
- `value` value part of color (0 - 255, rescaled from 0 - 1. e.g. value = 0.5, rescaled to 127)

**Returns:**

- ESP\_OK: Set HSV color for a specific pixel successfully
- ESP\_ERR\_INVALID\_ARG: Set HSV color for a specific pixel failed because of an invalid argument
- ESP\_FAIL: Set HSV color for a specific pixel failed because other error occurred

### function `led_strip_set_pixel_rgbw`

_Set RGBW for a specific pixel._

```c
esp_err_t led_strip_set_pixel_rgbw (
    led_strip_handle_t strip,
    uint32_t index,
    uint32_t red,
    uint32_t green,
    uint32_t blue,
    uint32_t white
)
```

**Note:**

Only call this function if your led strip does have the white component (e.g. SK6812-RGBW)

**Note:**

Also see `led_strip_set_pixel` if you only want to specify the RGB part of the color and bypass the white component

**Parameters:**

- `strip` LED strip
- `index` index of pixel to set
- `red` red part of color
- `green` green part of color
- `blue` blue part of color
- `white` separate white component

**Returns:**

- ESP\_OK: Set RGBW color for a specific pixel successfully
- ESP\_ERR\_INVALID\_ARG: Set RGBW color for a specific pixel failed because of an invalid argument
- ESP\_FAIL: Set RGBW color for a specific pixel failed because other error occurred

### function `led_strip_switch_gpio`

_Switch GPIO of LED strip._

```c
esp_err_t led_strip_switch_gpio (
    led_strip_handle_t strip,
    gpio_num_t new_gpio_num,
    bool invert_output
) 
```

**Parameters:**

- `strip` LED strip
- `new_gpio_num` new GPIO number
- `invert_output` invert output

**Note:**

Only support RMT backend now

**Returns:**

- ESP\_OK: Switch GPIO successfully
- ESP\_FAIL: Switch GPIO failed because some other error occurred

## File include/led_strip_parlio.h

## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**led\_strip\_parlio\_config\_t**](#struct-led_strip_parlio_config_t) <br>_LED Strip PARLIO specific configuration._ |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**led\_strip\_new\_parlio\_group**](#function-led_strip_new_parlio_group) (const [**led\_strip\_config\_t**](#struct-led_strip_config_t) \*led\_config, const [**led\_strip\_parlio\_config\_t**](#struct-led_strip_parlio_config_t) \*parlio\_config, [**led\_strip\_group\_handle\_t**](#typedef-led_strip_group_handle_t) \*ret\_group) <br>_Create LED strip group based on PARLIO\_TX unit._ |

## Structures and Types Documentation

### struct `led_strip_parlio_config_t`

_LED Strip PARLIO specific configuration._

Variables:

- parlio\_clock\_source\_t clk_src  <br>PARLIO clock source

- uint8\_t strip_count  <br>Number of LED strips. Should be a power of 2 and not larger than PARLIO\_TX\_UNIT\_MAX\_DATA\_WIDTH

- gpio\_num\_t strip_gpio_num  <br>GPIO number that used by LED strip

## Functions Documentation

### function `led_strip_new_parlio_group`

_Create LED strip group based on PARLIO\_TX unit._

```c
esp_err_t led_strip_new_parlio_group (
    const led_strip_config_t *led_config,
    const led_strip_parlio_config_t *parlio_config,
    led_strip_group_handle_t *ret_group
) 
```

**Note:**

The strip\_gpio\_num in led\_config no longer takes effect, and other configurations will be shared by all LED strips in the group.

**Parameters:**

- `led_config` LED strip configuration
- `parlio_config` PARLIO specific configuration
- `ret_group` Returned LED strip group handle

**Returns:**

- ESP\_OK: create LED strip handle successfully
- ESP\_ERR\_INVALID\_ARG: create LED strip handle failed because of invalid argument
- ESP\_ERR\_NOT\_SUPPORTED: create LED strip handle failed because of unsupported configuration
- ESP\_ERR\_NO\_MEM: create LED strip handle failed because of out of memory

## File include/led_strip_rmt.h

## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**led\_strip\_rmt\_config\_t**](#struct-led_strip_rmt_config_t) <br>_LED Strip RMT specific configuration._ |
| struct | [**led\_strip\_rmt\_extra\_config**](#struct-led_strip_rmt_config_tled_strip_rmt_extra_config) <br> |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**led\_strip\_new\_rmt\_device**](#function-led_strip_new_rmt_device) (const [**led\_strip\_config\_t**](#struct-led_strip_config_t) \*led\_config, const [**led\_strip\_rmt\_config\_t**](#struct-led_strip_rmt_config_t) \*rmt\_config, [**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) \*ret\_strip) <br>_Create LED strip based on RMT TX channel._ |

## Structures and Types Documentation

### struct `led_strip_rmt_config_t`

_LED Strip RMT specific configuration._

Variables:

- rmt\_clock\_source\_t clk_src  <br>RMT clock source

- struct [**led\_strip\_rmt\_config\_t::led\_strip\_rmt\_extra\_config**](#struct-led_strip_rmt_config_tled_strip_rmt_extra_config) flags  <br>Extra driver flags

- size\_t mem_block_symbols  <br>How many RMT symbols can one RMT channel hold at one time. Set to 0 will fallback to use the default size. Extra RMT specific driver flags

- uint32\_t resolution_hz  <br>RMT tick resolution, if set to zero, a default resolution (10MHz) will be applied

### struct `led_strip_rmt_config_t::led_strip_rmt_extra_config`

Variables:

- uint32\_t with_dma  <br>Use DMA to transmit data

## Functions Documentation

### function `led_strip_new_rmt_device`

_Create LED strip based on RMT TX channel._

```c
esp_err_t led_strip_new_rmt_device (
    const led_strip_config_t *led_config,
    const led_strip_rmt_config_t *rmt_config,
    led_strip_handle_t *ret_strip
)
```

**Parameters:**

- `led_config` LED strip configuration
- `rmt_config` RMT specific configuration
- `ret_strip` Returned LED strip handle

**Returns:**

- ESP\_OK: create LED strip handle successfully
- ESP\_ERR\_INVALID\_ARG: create LED strip handle failed because of invalid argument
- ESP\_ERR\_NO\_MEM: create LED strip handle failed because of out of memory
- ESP\_FAIL: create LED strip handle failed because some other error

## File include/led_strip_spi.h

## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**led\_strip\_spi\_config\_t**](#struct-led_strip_spi_config_t) <br>_LED Strip SPI specific configuration._ |
| struct | [**led\_strip\_spi\_extra\_config**](#struct-led_strip_spi_config_tled_strip_spi_extra_config) <br> |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**led\_strip\_new\_spi\_device**](#function-led_strip_new_spi_device) (const [**led\_strip\_config\_t**](#struct-led_strip_config_t) \*led\_config, const [**led\_strip\_spi\_config\_t**](#struct-led_strip_spi_config_t) \*spi\_config, [**led\_strip\_handle\_t**](#typedef-led_strip_handle_t) \*ret\_strip) <br>_Create LED strip based on SPI MOSI channel._ |

## Structures and Types Documentation

### struct `led_strip_spi_config_t`

_LED Strip SPI specific configuration._

Variables:

- spi\_clock\_source\_t clk_src  <br>SPI clock source

- struct [**led\_strip\_spi\_config\_t::led\_strip\_spi\_extra\_config**](#struct-led_strip_spi_config_tled_strip_spi_extra_config) flags  <br>Extra driver flags

- spi\_host\_device\_t spi_bus  <br>SPI bus ID. Which buses are available depends on the specific chip

### struct `led_strip_spi_config_t::led_strip_spi_extra_config`

Variables:

- uint32\_t with_dma  <br>Use DMA to transmit data

## Functions Documentation

### function `led_strip_new_spi_device`

_Create LED strip based on SPI MOSI channel._

```c
esp_err_t led_strip_new_spi_device (
    const led_strip_config_t *led_config,
    const led_strip_spi_config_t *spi_config,
    led_strip_handle_t *ret_strip
)
```

**Note:**

Although only the MOSI line is used for generating the signal, the whole SPI bus can't be used for other purposes.

**Parameters:**

- `led_config` LED strip configuration
- `spi_config` SPI specific configuration
- `ret_strip` Returned LED strip handle

**Returns:**

- ESP\_OK: create LED strip handle successfully
- ESP\_ERR\_INVALID\_ARG: create LED strip handle failed because of invalid argument
- ESP\_ERR\_NOT\_SUPPORTED: create LED strip handle failed because of unsupported configuration
- ESP\_ERR\_NO\_MEM: create LED strip handle failed because of out of memory
- ESP\_FAIL: create LED strip handle failed because some other error

## File include/led_strip_types.h

## Structures and Types

| Type | Name |
| ---: | :--- |
| union  | [**led\_color\_component\_format\_t**](#union-led_color_component_format_t)  <br>_LED color component format._ |
| struct | [**format\_layout**](#struct-led_color_component_format_tformat_layout) <br> |
| enum  | [**led\_model\_t**](#enum-led_model_t)  <br>_LED strip model._ |
| struct | [**led\_strip\_config\_t**](#struct-led_strip_config_t) <br>_LED Strip common configurations The common configurations are not specific to any backend peripheral._ |
| struct | [**led\_strip\_extra\_flags**](#struct-led_strip_config_tled_strip_extra_flags) <br> |
| struct | [**led\_strip\_encoder\_timings\_t**](#struct-led_strip_encoder_timings_t) <br>_LED strip encoder timings._ |
| typedef struct [**led\_strip\_group\_t**](#struct-led_strip_group_t) \* | [**led\_strip\_group\_handle\_t**](#typedef-led_strip_group_handle_t)  <br>_Type of LED strip group handle._ |
| typedef struct [**led\_strip\_t**](#struct-led_strip_t) \* | [**led\_strip\_handle\_t**](#typedef-led_strip_handle_t)  <br>_Type of LED strip handle._ |

## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LED\_STRIP\_COLOR\_COMPONENT\_FMT\_GRB**](#define-led_strip_color_component_fmt_grb)  ([**led\_color\_component\_format\_t**](#union-led_color_component_format_t)){.format = {.r\_pos = 1, .g\_pos = 0, .b\_pos = 2, .w\_pos = 3, .reserved = 0, .num\_components = 3}}<br>_Helper macros to set the color component format._ |
| define  | [**LED\_STRIP\_COLOR\_COMPONENT\_FMT\_GRBW**](#define-led_strip_color_component_fmt_grbw)  ([**led\_color\_component\_format\_t**](#union-led_color_component_format_t)){.format = {.r\_pos = 1, .g\_pos = 0, .b\_pos = 2, .w\_pos = 3, .reserved = 0, .num\_components = 4}}<br> |
| define  | [**LED\_STRIP\_COLOR\_COMPONENT\_FMT\_RGB**](#define-led_strip_color_component_fmt_rgb)  ([**led\_color\_component\_format\_t**](#union-led_color_component_format_t)){.format = {.r\_pos = 0, .g\_pos = 1, .b\_pos = 2, .w\_pos = 3, .reserved = 0, .num\_components = 3}}<br> |
| define  | [**LED\_STRIP\_COLOR\_COMPONENT\_FMT\_RGBW**](#define-led_strip_color_component_fmt_rgbw)  ([**led\_color\_component\_format\_t**](#union-led_color_component_format_t)){.format = {.r\_pos = 0, .g\_pos = 1, .b\_pos = 2, .w\_pos = 3, .reserved = 0, .num\_components = 4}}<br> |

## Structures and Types Documentation

### union `led_color_component_format_t`

_LED color component format._

**Note:**

The format is used to specify the order of color components in each pixel, also the number of color components.

Variables:

- struct [**led\_color\_component\_format\_t::format\_layout**](#struct-led_color_component_format_tformat_layout) format  <br>Format layout

- uint32\_t format_id  <br>Format ID

### struct `led_color_component_format_t::format_layout`

Variables:

- uint32\_t b_pos  <br>Position of the blue channel in the color order: 0~3

- uint32\_t g_pos  <br>Position of the green channel in the color order: 0~3

- uint32\_t num_components  <br>Number of color components per pixel: 3 or 4. If set to 0, it will fallback to 3

- uint32\_t r_pos  <br>Position of the red channel in the color order: 0~3

- uint32\_t reserved  <br>Reserved

- uint32\_t w_pos  <br>Position of the white channel in the color order: 0~3

### enum `led_model_t`

_LED strip model._

```c
enum led_model_t {
    LED_MODEL_WS2812,
    LED_MODEL_SK6812,
    LED_MODEL_WS2811,
    LED_MODEL_CUSTOM,
    LED_MODEL_INVALID
};
```

**Note:**

Different led model may have different timing parameters, so we need to distinguish them.

### struct `led_strip_config_t`

_LED Strip common configurations The common configurations are not specific to any backend peripheral._

Variables:

- [**led\_color\_component\_format\_t**](#union-led_color_component_format_t) color_component_format  <br>Specifies the order of color components in each pixel. Use helper macros like `LED_STRIP_COLOR_COMPONENT_FMT_GRB` to set the format

- struct [**led\_strip\_config\_t::led\_strip\_extra\_flags**](#struct-led_strip_config_tled_strip_extra_flags) flags  <br>Extra driver flags

- [**led\_model\_t**](#enum-led_model_t) led_model  <br>Specifies the LED strip model (e.g., WS2812, SK6812)

- uint32\_t max_leds  <br>Maximum number of LEDs that can be controlled in a single strip

- int strip_gpio_num  <br>GPIO number that used by LED strip

- [**led\_strip\_encoder\_timings\_t**](#struct-led_strip_encoder_timings_t) timings  <br>Encoder timings, only valid for RMT backend LED strip extra driver flags

### struct `led_strip_config_t::led_strip_extra_flags`

Variables:

- uint32\_t invert_out  <br>Invert output signal

### struct `led_strip_encoder_timings_t`

_LED strip encoder timings._

**Note:**

The logic timings are in nanoseconds and the reset timings is in microseconds.

Variables:

- uint32\_t reset  <br>Reset time, microseconds

- uint32\_t t0h  <br>High time for 0 bit, nanoseconds

- uint32\_t t0l  <br>Low time for 0 bit, nanoseconds

- uint32\_t t1h  <br>High time for 1 bit, nanoseconds

- uint32\_t t1l  <br>Low time for 1 bit, nanoseconds

### typedef `led_strip_group_handle_t`

_Type of LED strip group handle._

```c
typedef struct led_strip_group_t* led_strip_group_handle_t;
```

### typedef `led_strip_handle_t`

_Type of LED strip handle._

```c
typedef struct led_strip_t* led_strip_handle_t;
```

## Macros Documentation

### define `LED_STRIP_COLOR_COMPONENT_FMT_GRB`

_Helper macros to set the color component format._

```c
#define LED_STRIP_COLOR_COMPONENT_FMT_GRB ( led_color_component_format_t ){.format = {.r_pos = 1, .g_pos = 0, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 3}}
```

### define `LED_STRIP_COLOR_COMPONENT_FMT_GRBW`

```c
#define LED_STRIP_COLOR_COMPONENT_FMT_GRBW ( led_color_component_format_t ){.format = {.r_pos = 1, .g_pos = 0, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 4}}
```

### define `LED_STRIP_COLOR_COMPONENT_FMT_RGB`

```c
#define LED_STRIP_COLOR_COMPONENT_FMT_RGB ( led_color_component_format_t ){.format = {.r_pos = 0, .g_pos = 1, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 3}}
```

### define `LED_STRIP_COLOR_COMPONENT_FMT_RGBW`

```c
#define LED_STRIP_COLOR_COMPONENT_FMT_RGBW ( led_color_component_format_t ){.format = {.r_pos = 0, .g_pos = 1, .b_pos = 2, .w_pos = 3, .reserved = 0, .num_components = 4}}
```

## File interface/led_strip_interface.h

## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**led\_strip\_group\_t**](#struct-led_strip_group_t) <br>_LED strip group interface definition._ |
| typedef struct [**led\_strip\_group\_t**](#struct-led_strip_group_t) | [**led\_strip\_group\_t**](#typedef-led_strip_group_t)  <br> |
| struct | [**led\_strip\_t**](#struct-led_strip_t) <br>_LED strip interface definition._ |
| typedef struct [**led\_strip\_t**](#struct-led_strip_t) | [**led\_strip\_t**](#typedef-led_strip_t)  <br> |

## Structures and Types Documentation

### struct `led_strip_group_t`

_LED strip group interface definition._

Variables:

- esp\_err\_t(\* del  <br>_Free LED strip group resources._<br>**Parameters:**

- `group` LED strip group

**Returns:**

- ESP\_OK: Free resources successfully
- ESP\_FAIL: Free resources failed because error occurred

- esp\_err\_t(\* get_strip_handle  <br>_Get LED strip handle by index._<br>**Parameters:**

- `group` LED strip group
- `index` LED strip index
- `ret_strip` Returned LED strip handle

**Returns:**

- ESP\_OK: Success
- ESP\_ERR\_INVALID\_ARG: Invalid argument

### typedef `led_strip_group_t`

```c
typedef struct led_strip_group_t led_strip_group_t;
```

Type of LED group strip

### struct `led_strip_t`

_LED strip interface definition._

Variables:

- esp\_err\_t(\* clear  <br>_Clear LED strip (turn off all LEDs)_<br>**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Clear LEDs successfully
- ESP\_FAIL: Clear LEDs failed because some other error occurred

- esp\_err\_t(\* del  <br>_Free LED strip resources._<br>**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Free resources successfully
- ESP\_FAIL: Free resources failed because error occurred

- esp\_err\_t(\* refresh  <br>_Refresh memory colors to LEDs._<br>**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Refresh successfully
- ESP\_FAIL: Refresh failed because some other error occurred

**Note:**

After updating the LED colors in the memory, a following invocation of this API is needed to flush colors to strip.

- esp\_err\_t(\* refresh_async  <br>_Refresh memory colors to LEDs asynchronously._<br>**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Refresh successfully
- ESP\_FAIL: Refresh failed because some other error occurred

**Note:**

This function is non-blocking, so you need to call `led_strip_refresh_wait_async_done` to wait for the refresh to complete before modifying the LED colors again.

- esp\_err\_t(\* refresh_wait_async_done  <br>_Wait for the async refresh to complete._<br>**Parameters:**

- `strip` LED strip

**Returns:**

- ESP\_OK: Wait for the async refresh to complete successfully

- esp\_err\_t(\* set_pixel  <br>_Set RGB for a specific pixel._<br>**Parameters:**

- `strip` LED strip
- `index` index of pixel to set
- `red` red part of color
- `green` green part of color
- `blue` blue part of color

**Returns:**

- ESP\_OK: Set RGB for a specific pixel successfully
- ESP\_ERR\_INVALID\_ARG: Set RGB for a specific pixel failed because of invalid parameters
- ESP\_FAIL: Set RGB for a specific pixel failed because other error occurred

- esp\_err\_t(\* set_pixel_rgbw  <br>_Set RGBW for a specific pixel. Similar to_ `set_pixel`_but also set the white component._<br>**Parameters:**

- `strip` LED strip
- `index` index of pixel to set
- `red` red part of color
- `green` green part of color
- `blue` blue part of color
- `white` separate white component

**Returns:**

- ESP\_OK: Set RGBW color for a specific pixel successfully
- ESP\_ERR\_INVALID\_ARG: Set RGBW color for a specific pixel failed because of an invalid argument
- ESP\_FAIL: Set RGBW color for a specific pixel failed because other error occurred

- esp\_err\_t(\* switch_gpio  <br>_Switch GPIO of LED strip._<br>**Parameters:**

- `strip` LED strip
- `new_gpio_num` new GPIO number
- `invert_output` invert output

**Note:**

Only support RMT backend now

**Returns:**

- ESP\_OK: Switch GPIO successfully
- ESP\_FAIL: Switch GPIO failed because some other error occurred

### typedef `led_strip_t`

```c
typedef struct led_strip_t led_strip_t;
```

Type of LED strip
