# CST820 get-started example

This example prints the panel-IO defaults without touching hardware. The
coordinate maxima in `esp_lcd_touch_config_t` are inclusive (`459` for a
460-pixel axis). The driver supplies the common framework sleep hooks and its
monitor-mode helpers keep the interrupt wake path available.
