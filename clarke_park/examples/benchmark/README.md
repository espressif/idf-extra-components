# Clarke-Park benchmark

Measures the cost of every `clarke_park` transform with the CPU cycle counter
(`esp_cpu_get_cycle_count()`).

Each transform is called 10000 times with a varying input vector (and, for the
Park transforms, a varying angle) so that nothing can be hoisted out of the
loop. The result is printed as average cycles per call, the time at the
configured CPU frequency and the load in a 16 kHz control loop.

## Build and run

```
idf.py -C examples/benchmark -B build_esp32s3 build flash monitor -p (PORT)
```

The example builds with `-O2` and a fixed CPU frequency
(`sdkconfig.defaults`), because cycle counts are only comparable when the
compiler and the clock are the same. The `sdkconfig.defaults` also pins
`CONFIG_CLARKE_PARK_IQ_FORMAT=15`, so the numbers are always produced with the
format the component ships with and with the one the CORDIC hardware supports.
The banner it prints always states which configuration the numbers belong to.

## Comparing the backends

The Q-format is a compile time property, so a single build only measures one
format. These are the interesting configurations:

| Build | What it shows |
| --- | --- |
| `sdkconfig.defaults` (Q15) | IQmath software `sin`/`cos` at Q15, the component default |
| `CONFIG_CLARKE_PARK_IQ_FORMAT=24` | IQmath software `sin`/`cos` at high precision |
| Q15 + `CONFIG_CLARKE_PARK_USE_CORDIC_HW` | CORDIC hardware path, plus a `park_iq, IQmath sin/cos` reference line |

Example output (Q15 + CORDIC on ESP32-S3R at 240 MHz):

```
--- IQmath backend (Q15) ---
clarke_iq                     11 cycles      45 ns    0.0 %
iclarke_iq                     9 cycles      37 ns    0.0 %
park_iq                       42 cycles     175 ns    0.2 %
ipark_iq                      44 cycles     183 ns    0.3 %

--- Q15 reference without CORDIC ---
park_iq, IQmath sin/cos      280 cycles    1166 ns    1.8 %
```

The numbers above are only an illustration: run the example on your target to
get the real ones.

## Interpreting the numbers

- The Clarke transforms are a handful of cycles: pure fixed-point arithmetic
  without any trigonometry.
- The Park transforms are dominated by `sin`/`cos`, so they cost far more than
  the Clarke transforms in the software backend.
- The float backend pays for the `sinf`/`cosf` calls, which is why it can be
  slower than a Q15 fixed-point Park transform on a core without an FPU.
- With `CONFIG_CLARKE_PARK_USE_CORDIC_HW` enabled and Q15, the
  `park_iq, IQmath sin/cos` line is the software reference: the difference to
  the `park_iq` line above is what the CORDIC hardware saves.

This is a first order comparison tool: it does not flush the caches and does
not disable interrupts, so treat the numbers as indicative, not as cycle
accurate measurements.
