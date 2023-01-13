## 1.0.0

- Initial driver version, based on the legacy RMT driver (`driver/rmt.h`)

## 2.0.0

- Reimplemented the driver using the new RMT driver (`driver/rmt_tx.h`)

## 2.1.0

- Support DMA feature, which offloads the CPU by a lot when it comes to drive a bunch of LEDs
- Support various RMT clock sources
- Acquire and release the power management lock before and after each refresh
- New driver flag: `invert_out` which can invert the led control signal by hardware

## 2.2.0

- Support for 4 components RGBW leds (SK6812):
  - new field led_type in led_strip_config_t flags; valid values are
      LED_TYPE_WS2812
      LED_TYPE_SK6812
  - new API led_strip_set_pixel_rgbw
  - new interface type set_pixel_rgbw
  - using specific timing for SK6812
