# Proportional integral derivative controller

[![Component Registry](https://components.espressif.com/components/espressif/pid_ctrl/badge.svg)](https://components.espressif.com/components/espressif/pid_ctrl)

## Numeric backends

Two numeric backends are built in and can coexist in the same firmware:

- **float** — `pid_*_f()` APIs with `pid_ctrl_config_f_t` / `pid_ctrl_block_f_handle_t`.
- **IQmath `_iq`** — `pid_*_iq()` APIs with `pid_ctrl_config_iq_t` / `pid_ctrl_block_iq_handle_t`.

### Unified interface

For convenience, an unsuffixed API (`pid_new_control_block`, `pid_compute`, ...) is provided that automatically dispatches to the correct backend based on argument types:

- In **C**, `_Generic` selects between `_f` and `_iq` based on the handle / config pointer type.
- In **C++**, the same names are provided as overloaded `static inline` wrappers, so the same call site works for either backend.

The unsuffixed type aliases (`pid_ctrl_config_t`, `pid_ctrl_parameter_t`, `pid_ctrl_block_handle_t`) map to the float variants, matching the behavior of the original single-backend API.

### IQmath Q-format

The fractional bit count used by the `_iq` backend is controlled by `CONFIG_PID_CTRL_IQ_FORMAT` (Kconfig, default `24`, range `1..30`). It is applied as `GLOBAL_IQ` before `IQmathLib.h` is included. If your application `#define`s `GLOBAL_IQ` itself before including `pid_ctrl.h`, your definition wins and the Kconfig value is ignored.
