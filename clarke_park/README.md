# Clarke Park Transform

[![Component Registry](https://components.espressif.com/components/espressif/clarke_park/badge.svg)](https://components.espressif.com/components/espressif/clarke_park)

Three phase currents that all wiggle? The Clarke and Park transforms turn them into two calm numbers your control loop can actually use. This component gives you both transforms (plus their inverses) in `float` and in `IQmath` fixed-point, behind a single API.

## What you get

- **Clarke / inverse Clarke** - three phases (U/V/W) to two stator axes (alpha/beta) and back.
- **Park / inverse Park** - stator axes to rotor axes (d/q) and back, so that a constant-speed machine gives you nearly constant values.
- **Two numeric backends in the same firmware** - `float` for convenience, IQmath fixed-point for speed. Pick the backend with the coordinate type you pass in; the calls do not change.
- **Every IQmath Q-format from `_iq8` to `_iq28` at the same time** - the format is picked from the types you pass in, so nothing has to be configured. Unused formats are dropped by the linker.
- **Nothing to allocate, nothing to initialize** - the transforms are pure math.

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

`theta` is the electrical angle in radians: a `float` for the float backend, an `_iq` value for the unsuffixed fixed-point backend.

## IQmath Q-formats

The Q-format is selected by the **coordinate type**. Different formats can be used in one firmware, including in the same translation unit:

```c
clarke_park_uvw_iq15_t uvw15 = { .u = _IQ15(1.0f), .v = _IQ15(-0.5f), .w = _IQ15(-0.5f) };
clarke_park_ab_iq15_t ab15;
clarke_park_clarke(&uvw15, &ab15);          /* Q15 arithmetic */

clarke_park_uvw_iq8_t uvw8 = { .u = _IQ8(1.0f), .v = _IQ8(-0.5f), .w = _IQ8(-0.5f) };
clarke_park_ab_iq8_t ab8;
clarke_park_clarke(&uvw8, &ab8);            /* Q8 arithmetic */
```

The unsuffixed `_iq` API follows `GLOBAL_IQ` of the translation unit that includes `clarke_park.h`. The wrappers do not freeze the format at whatever `GLOBAL_IQ` happened to be when this component was compiled. `_iq1`..`_iq7` / `GLOBAL_IQ<8`, `_iq29` / `GLOBAL_IQ=29` and `_iq30` / `GLOBAL_IQ=30` are not supported.

> [!NOTE]
> IQmath `_iqN` scalars are all `int32_t`, so Park's angle must be converted with the matching `_IQN()` helper.
