# Eigen: A Header-Only Linear Algebra Library for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/eigen/badge.svg)](https://components.espressif.com/components/espressif/eigen)

[Eigen](https://libeigen.gitlab.io/) is a header-only C++ library for linear algebra. It provides matrices, vectors, numerical solvers, decompositions, geometry, and related algorithms.

## Component status

- Provides Eigen's header-only C++ API and modules.
- Requires ESP-IDF 5.0 or newer and C++14 or newer.
- Does not build or provide the optional BLAS/LAPACK compatibility libraries in the upstream repository.

## Usage

Add the component as a dependency:

```yaml
dependencies:
  espressif/eigen: "^5.0.1"
```

This component provides the upstream Eigen API without ESP-IDF-specific wrappers.

Include Eigen headers directly from a C++ source file. For the complete Eigen API, include the umbrella header:

```cpp
#include "Eigen/Eigen"

void calculate()
{
    Eigen::Matrix2f matrix;
    matrix << 1.0f, 2.0f,
              3.0f, 4.0f;
}
```

For smaller builds, include only the modules that are needed, for example `"Eigen/Core"`, `"Eigen/Dense"`, or `"Eigen/Geometry"`.

See the [get-started example](examples/get_started/) for a complete ESP-IDF project.

## Learn more

For API documentation and usage details, refer to the upstream Eigen documentation:

- [Eigen documentation](https://libeigen.gitlab.io)
- [Eigen repository](https://gitlab.com/libeigen/eigen)
