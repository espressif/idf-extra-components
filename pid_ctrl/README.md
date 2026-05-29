# Proportional integral derivative controller

[![Component Registry](https://components.espressif.com/components/espressif/pid_ctrl/badge.svg)](https://components.espressif.com/components/espressif/pid_ctrl)

## Numeric backends

Two numeric backends are built in and can coexist in the same firmware:

- **float** — `pid_*_f()` APIs with `pid_ctrl_config_f_t` / `pid_ctrl_block_handle_f_t`.
- **IQmath `_iq`** — `pid_*_iq()` APIs with `pid_ctrl_config_iq_t` / `pid_ctrl_block_handle_iq_t`.

### Unified interface

For convenience, an unsuffixed API (`pid_new_control_block`, `pid_compute`, ...) is provided that automatically dispatches to the correct backend based on argument types:

- Use `pid_ctrl_config_f_t` and `pid_ctrl_block_handle_f_t` for the float backend.
- Use `pid_ctrl_config_iq_t` and `pid_ctrl_block_handle_iq_t` for the IQmath backend.

### IQmath Q-format

The fractional bit count used by the `_iq` backend is controlled by `CONFIG_PID_CTRL_IQ_FORMAT` (Kconfig, default `24`, range `1..30`). If your application defines `GLOBAL_IQ` before including `pid_ctrl.h`, it must match `CONFIG_PID_CTRL_IQ_FORMAT`.