# LED Strip Driver

[![Component Registry](https://components.espressif.com/components/espressif/led_strip/badge.svg)](https://components.espressif.com/components/espressif/led_strip)

This driver is designed for addressable LEDs like [WS2812](http://www.world-semi.com/Certifications/WS2812B.html), where each LED is controlled by a single data line.

## Backend Controllers

### The [RMT](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html) Peripheral

This is the most economical way to drive the LEDs because it only consumes one RMT channel, leaving other channels free to use. However, the memory usage increases dramatically with the number of LEDs. If the RMT hardware can't be assist by DMA, the driver will going into interrupt very frequently, thus result in a high CPU usage. What's worse, if the RMT interrupt is delayed or not serviced in time (e.g. if Wi-Fi interrupt happens on the same CPU core), the RMT transaction will be corrupted and the LEDs will display incorrect colors. If you want to use RMT to drive a large number of LEDs, you'd better to enable the DMA feature if possible [^1].

#### Allocate LED Strip Object with RMT Backend

```c
#define BLINK_GPIO 0

/// LED strip common configuration
led_strip_config_t strip_config = {
    .strip_gpio_num = BLINK_GPIO,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = {
        .invert_out = false, // don't invert the output signal
    }
};

/// RMT backend specific configuration
led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
    .flags = {
        .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
};

/// Create the LED strip object
led_strip_handle_t led_strip;
ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
```

You can create multiple LED strip objects with different GPIOs and pixel numbers. The backend driver will automatically allocate the RMT channel for you if there is more available.

### The [SPI](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html) Peripheral

SPI peripheral can also be used to generate the timing required by the LED strip. However this backend is not as economical as the RMT one, because it will take up the whole **bus**, unlike the RMT just takes one **channel**. You **CANT** connect other devices to the same SPI bus if it's been used by the led_strip, because the led_strip doesn't have the concept of "Chip Select".

Please note, the SPI backend has a dependency of **ESP-IDF >= 5.1**

#### Allocate LED Strip Object with SPI Backend

```c
#define BLINK_GPIO 0

/// LED strip common configuration
led_strip_config_t strip_config = {
    .strip_gpio_num = BLINK_GPIO,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = {
        .invert_out = false, // don't invert the output signal
    }
};

/// SPI backend specific configuration
led_strip_spi_config_t spi_config = {
    .clk_src = SPI_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
    .spi_bus = SPI2_HOST,           // SPI bus ID
    .flags = {
        .with_dma = true, // Using DMA can improve performance and help drive more LEDs
    }
};

/// Create the LED strip object
led_strip_handle_t led_strip;
ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
```

The number of LED strip objects can be created depends on how many free SPI buses are free to use in your project.

### The [Parallel IO](https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/parlio.html) Peripheral

Parallel IO peripheral can also be used to generate the timing required by the LED strip. Since the Parallel IO peripheral is a parallel interface, we manage it through groups. The number of LED strips supported in a strip group depends on the maximum data width of the Parallel IO tx_unit.
The strip_handle obtained through **get_strip_handle** API is consistent with the handles used in RMT backend and SPI backend. However, it cannot be deleted individually. Call **group_del** API to delete the entire group.

Please note, the Parallel IO backend has a dependency of **ESP-IDF >= 5.1**

#### Allocate LED Strip Object with Parallel IO Backend

```c
#define LED_STRIP_COUNT 4
#define LED_STRIP0_GPIO_PIN 0
#define LED_STRIP1_GPIO_PIN 1
#define LED_STRIP2_GPIO_PIN 2
#define LED_STRIP3_GPIO_PIN 3

/// LED strip common configuration
led_strip_config_t strip_config = {
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = {
        .invert_out = false, // don't invert the output signal
    }
};

/// Parallel IO backend specific configuration
led_strip_parlio_config_t parlio_config = {
    .clk_src = PARLIO_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
    .strip_count = LED_STRIP_COUNT,
    .strip_gpio_num = {
                        LED_STRIP0_GPIO_PIN,
                        LED_STRIP1_GPIO_PIN,
                        LED_STRIP2_GPIO_PIN,
                        LED_STRIP3_GPIO_PIN,
                        ...,
                      },
};

/// Create the LED strip group object
led_strip_group_handle_t parlio_group;
ESP_ERROR_CHECK(led_strip_new_parlio_group(&strip_config, &parlio_config, &parlio_group));

/// get the LED strip object
led_strip_handle_t *led_strip = calloc(LED_STRIP_COUNT, sizeof(led_strip_handle_t));
for(int i = 0; i < LED_STRIP_COUNT; i++) {
    ESP_ERROR_CHECK(led_strip_group_get_strip_handle(parlio_group, i, &led_strip[i]));
}
```

The number of LED strip group objects can be created depends on how many free Parallel IO TX unit are free to use in your project.

## FAQ

* Which led_strip backend should I choose?
  * It depends on your application requirement and target chip's ability.

    ```mermaid
    flowchart LR
    A{Is RMT supported?}
    A --> |No| B[SPI backend]
    B --> C{Does the led strip has \n a larger number of LEDs?}
    C --> |No| D[Don't have to enable the DMA of the backend]
    C --> |Yes| E[Enable the DMA of the backend]
    A --> |Yes| F{Does the led strip has \n a larger number of LEDs?}
    F --> |Yes| G{Does RMT support DMA?}
    G --> |Yes| E
    G --> |No| B
    F --> |No| H[RMT backend] --> D
    ```

* How to set the brightness of the LED strip?
  * You can tune the brightness by scaling the value of each R-G-B element with a **same** factor. But pay attention to the overflow of the value.

[^1]: The RMT DMA feature is not available on all ESP chips. Please check the data sheet before using it.
