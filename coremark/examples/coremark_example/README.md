# CoreMark example

This example can be used to run CoreMark benchmark on an Espressif chip.

The example doesn't require any special hardware and can run on any development board.

## Building and running

Run the application as usual for an ESP-IDF project. For example, for ESP32-C3:
```
idf.py set-target esp32c3
idf.py -p PORT flash monitor
```

After launching, the benchmark takes a few seconds to run, please be patient.

## Example output

Running on ESP32-C3, we can obtain the following output:

```
Running coremark...
2K performance run parameters for coremark.
CoreMark Size    : 666
Total ticks      : 14661
Total time (secs): 14.661000
Iterations/Sec   : 409.249028
Iterations       : 6000
Compiler version : GCC12.2.0
Compiler flags   : -ffunction-sections -fdata-sections -gdwarf-4 -ggdb -nostartfiles -nostartfiles -Og -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion -std=gnu17 -O3 -fjump-tables -ftree-switch-conversion
Memory location  : IRAM
seedcrc          : 0xe9f5
[0]crclist       : 0xe714
[0]crcmatrix     : 0x1fd7
[0]crcstate      : 0x8e3a
[0]crcfinal      : 0xa14c
Correct operation validated. See README.md for run and reporting rules.
CoreMark 1.0 : 409.249028 / GCC12.2.0 -ffunction-sections -fdata-sections -gdwarf-4 -ggdb -nostartfiles -nostartfiles -Og -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion -std=gnu17 -O3 -fjump-tables -ftree-switch-conversion / IRAM
CPU frequency: 160 MHz
```
