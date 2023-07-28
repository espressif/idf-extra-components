[![Component Registry](https://components.espressif.com/components/espressif/coremark/badge.svg)](https://components.espressif.com/components/espressif/coremark)

# Coremark for ESP-IDF

This component is a port of [CoreMark® benchmark](https://github.com/eembc/coremark) to ESP-IDF. It handles compiling CoreMark source files, providing necessary functions to measure timestamps, and enables various compiler to get higher performance.

# Using the benchmark

If you want to run the benchmark and see the results, create a demo project from the provided example:

```bash
idf.py create-project-from-example "espressif/coremark:coremark_example"
```

You can then build the project in `coremark_example` directory as usual. For example, to build the project for ESP32-C3:

```bash
cd coremark_example
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

(where `PORT` is the name of the serial port)

Refer to ESP-IDF Getting Started Guide for more information about compiling and running a project.

# Using as a component

You can also integrate CoreMark code into you project by adding dependency on `espressif/coremark` component:

```bash
idf.py add-dependency espressif/coremark
```

CoreMark benchmark entry point is an `int main(void)` function, which you can call from your application.

# Performance tweaks

This example does the following things to improve the benchmark result:

1. Enables `-O3` compiler flag for CoreMark source files.
2. Adds `-fjump-tables -ftree-switch-conversion` compiler flags for CoreMark source files. This overrides `-fno-jump-tables -fno-tree-switch-conversion` flags which get set in ESP-IDF build system by default.
3. Places CoreMark code into internal instruction RAM using [linker.lf](linker.lf) file.

For general information about optimizing performance of ESP-IDF applications, see the ["Performance" chapter of the Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/index.html).

# Example output

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

# Legal

CoreMark is a trademark of EEMBC and EEMBC is a registered trademark of the Embedded Microprocessor Benchmark Consortium.

CoreMark source code is Copyright (c) 2009 EEMBC. The source code is distributed under Apache 2.0 license with additional restrictions with regards to the use of the benchmark. See [LICENSE.md](LICENSE.md) for more details.

Any additional code in this component ("port layer") is Copyright (c) 2022-2023 Espressif Systems (Shanghai) Co. Ltd. and is licensed under Apache 2.0 license.



