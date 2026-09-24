# Clarke-Park Benchmark Example

Measures Clarke / Park cycle cost on three backends (`float`, Q15 IQmath, Q15 CORDIC) and checks that CORDIC Park / iPark stay within a few LSB of IQmath, including angles outside `[0, 2π)`.

## Usage

See [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started) for the full build flow.

### Hardware Required

- An ESP32-S31 board (needs the CORDIC peripheral)
- A USB cable for power and programming

### Software Required

- ESP-IDF v6.1 or later

### Set Chip Target

```
idf.py set-target esp32s31
```

### Configure the Project

`-O2` is pre-set in [sdkconfig.defaults](./sdkconfig.defaults) so the cycle counts are comparable.

### Build and Flash

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, use `Ctrl` + `]`.

## Example Output

```text
------------------------------------------------------------------------
 CORDIC correctness check   Fail if |d| > 4 LSB
------------------------------------------------------------------------
 Park / iPark   d LSB  (CORDIC - IQmath)
       theta       d       q   alpha    beta
           0      -1      +1      +1      -1
         ...
    -2pi-0.5      -1      +1      +1      -1

  max |d|  1 LSB
  result   PASS

------------------------------------------------------------------------
 Performance esp32s31 @320 MHz GCC -O2
------------------------------------------------------------------------
              float         IQmath        CORDIC
              cyc     ns    cyc     ns    cyc     ns
  clarke      235    734     81    253     81    253
  iclarke     512   1600     56    175     56    175
  park       1357   4241    446   1394    780   2438
  ipark      1361   4253    446   1394    782   2444
------------------------------------------------------------------------
 clarke_park benchmark finished
```

Each row is one transform. Each column is a backend. `cyc` is the average CPU cycles per call (10000 iterations, after one warmup run). `ns` is that time at the configured CPU clock.

- **clarke / iclarke** — IQmath and CORDIC match. CORDIC does not accelerate these; both columns are the IQmath path.
- **park / ipark (float)** — slowest. Most of the time is `sinf` / `cosf`.
- **park / ipark (IQmath)** — fastest here: table sin/cos, no peripheral setup.
- **park / ipark (CORDIC)** — between float and IQmath. The hardware Cos is cheap, but each call still does angle conversion, driver setup, polling and unpack. For one transform per call that overhead wins over IQmath.

The check table is CORDIC minus IQmath in LSB. A wrap or sign-extend bug is thousands of LSB and panics. A few LSB is expected (4 CORDIC iterations).

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/idf-extra-components/issues) on GitHub.
