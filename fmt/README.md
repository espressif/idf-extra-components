# fmt: fast and safe text formatting for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/fmt/badge.svg)](https://components.espressif.com/components/espressif/fmt)

**fmt** is an open-source formatting library providing a fast and safe alternative to C stdio and C++ iostreams.

This component packages the upstream [fmt](https://github.com/fmtlib/fmt) library for ESP-IDF. It does not add an ESP-specific wrapper: include the upstream headers and call the upstream API.

## Usage

Add the component to your project:

```bash
idf.py add-dependency "espressif/fmt^12.2.0"
```

Then use it from C++:

```cpp
#include <fmt/core.h>

fmt::print("Hello, {}!\n", "fmt");
```

See the [`examples/hello_fmt`](examples/hello_fmt) project for a complete ESP-IDF example.

## Learn more

For the API, format string syntax, and feature details, use the upstream documentation:

- [fmt documentation](https://fmt.dev/latest/index.html)
- [fmt repository](https://github.com/fmtlib/fmt)
