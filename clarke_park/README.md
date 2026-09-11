# Clarke Park Transform

[![Component Registry](https://components.espressif.com/components/espressif/clarke_park/badge.svg)](https://components.espressif.com/components/espressif/clarke_park)

Three phase currents that all wiggle? The Clarke and Park transforms turn them
into two calm numbers your control loop can actually use. This component gives
you both transforms (plus their inverses) in `float` and in IQmath fixed-point,
behind a single API - and can hand the trigonometry to the CORDIC hardware on
chips that have it.

## What you get

- **Clarke / inverse Clarke** - three phases (U/V/W) to two stator axes
  (alpha/beta) and back.
- **Park / inverse Park** - stator axes to rotor axes (d/q) and back, so that a
  constant-speed machine gives you nearly constant values.
- **Two numeric backends in the same firmware** - `float` for convenience,
  IQmath fixed-point for speed. Pick the backend with the coordinate type you
  pass in; the calls do not change.
- **CORDIC hardware acceleration** - optional, off by default, for the
  fixed-point Park transform. It is a Kconfig away on ESP32-S3R.
- **Nothing to allocate, nothing to initialize** - the transforms are pure math.
  The only exception is the CORDIC engine, which the component owns for you.

## Add it to your project

```bash
idf.py add-dependency "espressif/clarke_park"
```

## Quick start

```c
#include "clarke_park.h"

void example(float theta_rad)
{
    clarke_park_uvw_f_t uvw = { .u = 1.0f, .v = -0.5f, .w = -0.5f };
    clarke_park_ab_f_t ab;
    clarke_park_dq_f_t dq;

    clarke_park_clarke(&uvw, &ab);         // U/V/W  -> alpha/beta
    clarke_park_park(theta_rad, &ab, &dq); // alpha/beta -> d/q

    // the fixed-point backend uses the very same calls
    clarke_park_uvw_iq_t uvw_iq = { .u = _IQ(1.0f), .v = _IQ(-0.5f), .w = _IQ(-0.5f) };
    clarke_park_ab_iq_t ab_iq;
    clarke_park_dq_iq_t dq_iq;

    clarke_park_clarke(&uvw_iq, &ab_iq);
    clarke_park_park(_IQ(theta_rad), &ab_iq, &dq_iq);
}
```

`theta` is the electrical angle in radians: a `float` for the float backend, an
`_iq` value for the fixed-point backend.

## Configuration

| Kconfig option | Default | What it does |
| --- | --- | --- |
| `CONFIG_CLARKE_PARK_IQ_FORMAT` | `15` | Q-format (`GLOBAL_IQ`) of the fixed-point backend, range `1..30`. Q15 is the default because it matches the CORDIC hardware. |
| `CONFIG_CLARKE_PARK_USE_CORDIC_HW` | `n` | Use the CORDIC hardware for the `sin`/`cos` of the fixed-point Park transform. Only on chips with the peripheral, and only effective at Q15. |

## Where to go next

The full story - what the transforms do, the formulas, the Q-format trade-offs,
thread and ISR safety, accuracy and benchmark numbers - lives in the
programming guide:

- **Programming Guide and API Reference**:
  [Clarke Park Transform Documentation](https://espressif.github.io/idf-extra-components/latest/clarke_park/index.html)

Want to see the transforms run on your own chip?

```
idf.py -C examples/benchmark -B build_esp32s3 build flash monitor -p (PORT)
```
