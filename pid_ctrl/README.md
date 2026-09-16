# Proportional integral derivative controller

[![Component Registry](https://components.espressif.com/components/espressif/pid_ctrl/badge.svg)](https://components.espressif.com/components/espressif/pid_ctrl)

## Numeric backends

Two numeric backends are built in and can coexist in the same firmware:

- **float** — `pid_*_f()` with `pid_ctrl_config_f_t` / `pid_ctrl_block_handle_f_t`
- **IQmath** — one API per Q-format (`_iq1` .. `_iq30`): `pid_*_iqN()` with `pid_ctrl_config_iqN_t` / `pid_ctrl_block_handle_iqN_t`, plus an unsuffixed `_iq` API that follows `GLOBAL_IQ`

The Q-format is selected by the **type** you pass in. There is nothing to configure. Unused formats are dropped by the linker.

```c
pid_ctrl_config_iq24_t cfg24 = { .init_param = { .kp = _IQ24(1.0f), /* ... */ } };
pid_ctrl_block_handle_iq24_t h24 = NULL;
pid_new_control_block(&cfg24, &h24);

pid_ctrl_config_iq8_t cfg8 = { .init_param = { .kp = _IQ8(1.0f), /* ... */ } };
pid_ctrl_block_handle_iq8_t h8 = NULL;
pid_new_control_block(&cfg8, &h8);

_iq24 out = 0;
pid_compute(h24, _IQ24(0.5f), &out);   /* Q24 arithmetic */
```

The unsuffixed `_iq` API follows `GLOBAL_IQ` of the translation unit that includes `pid_ctrl.h`. It is an inline wrapper around the matching `_iqN` backend, so it does not freeze the format at whatever `GLOBAL_IQ` happened to be when this component was compiled.

```c
pid_ctrl_config_iq_t cfg = { .init_param = { .kp = _IQ(1.0f), /* ... */ } };
pid_ctrl_block_handle_iq_t h = NULL;
pid_new_control_block(&cfg, &h);

_iq out = 0;
pid_compute(h, _IQ(0.5f), &out);       /* GLOBAL_IQ arithmetic */
```

`pid_ctrl_config_iq_t` is a distinct type from `pid_ctrl_config_iqN_t`, so the unsuffixed `_iq` control block and an explicit `_iqN` one can be used in the same translation unit.

IQmath `_iqN` scalars are all `int32_t`. Mixing `_IQ8(...)` with a Q24 handle compiles and silently mis-scales; convert error / result / parameters with the matching `_IQN()` helper. Config structs and handles are a distinct type per format, so mixing those is a compile-time error.

### Unified interface

`pid_new_control_block`, `pid_compute`, ... dispatch from the argument type: float, unsuffixed `_iq`, or a concrete `_iqN`. This works in C (`_Generic`) and C++ (overloads).
