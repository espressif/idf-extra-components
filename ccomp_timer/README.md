# Cache Compensated Timer

[![Component Registry](https://components.espressif.com/components/espressif/ccomp_timer/badge.svg)](https://components.espressif.com/components/espressif/ccomp_timer)

The **Cache Compensated Timer** is a timer that tries to account for instruction and data stall cycles caused by cache misses. It is useful for measuring the time spent in a function or a block of code.

On Xtensa targets (e.g. ESP32), the timer is built on top of the debug module's performance monitor counter.

Due to hardware limitations, on RISC-V targets this driver falls back to using the CPU's cycle counter, which actually **doesn't** account for the cache misses. To achieve a measurement that is independent of cache misses you could place the code is to be measured into IRAM.
