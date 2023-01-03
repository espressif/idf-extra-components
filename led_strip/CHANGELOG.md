## 1.0.0

- Initial driver version, based on the legacy RMT driver (`driver/rmt.h`)

## 2.0.0

- Reimplemented the driver using the new RMT driver (`driver/rmt_tx.h`)

## 2.1.0

- Support DMA feature, which offloads the CPU by a lot when it comes to drive a bunch of LEDs
- Support various RMT clock sources
- Acquire and release the power management lock before and after each refresh
- New driver flag: `invert_out` which can invert the led control signal by hardware
