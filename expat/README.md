# Expat: XML Parsing C Library

[![Component Registry](https://components.espressif.com/components/espressif/expat/badge.svg)](https://components.espressif.com/components/espressif/expat)

[Expat](https://github.com/libexpat/libexpat) is a streaming XML 1.0 parser. You register callbacks and feed it bytes as they arrive; it does not build a document tree, so memory use stays proportional to the nesting depth rather than the file size.

This component is that library in the ESP-IDF component manager. It does not add a wrapper API: include `expat.h` and call the upstream C interface.

## What this component adds

On top of upstream libexpat, the packaging does the following for application authors:

- **menuconfig** — `CONFIG_EXPAT_XML_GE` and `CONFIG_EXPAT_XML_DTD` turn general-entity expansion and DTD handling on or off at compile time. Both default on. DTD support is only offered while general entities are on, which is an upstream requirement.
- **Matching headers** — those same feature macros are applied when your code includes `expat.h`, so the declarations you see match the library that was built. You do not need to include `expat_config.h` or pass `-D` flags yourself.
- **Entropy on chip** — Expat salts its hash tables against collision attacks. Chip builds use IDF's `getrandom()` (`esp_fill_random()` underneath).

Namespaces stay on. The character type stays UTF-8 `char`. Other upstream CMake knobs are not exposed.

## Add it to your project

```bash
idf.py add-dependency "espressif/expat^2.9.0"
```

Or list it in `main/idf_component.yml`:

```yaml
dependencies:
  espressif/expat: "^2.9.0"
```

Then include the upstream header:

```c
#include "expat.h"
```

## Configuration

The two options live under **Component config → expat** in `idf.py menuconfig`.

| Option | Default | Effect |
| --- | --- | --- |
| `CONFIG_EXPAT_XML_GE` | on | Expand general entities (`&name;`). The billion-laughs and allocation-tracker APIs are compiled in with this. Off: smaller parser; only the five predefined entities (`amp`, `lt`, `gt`, `apos`, `quot`) still expand, other `&name;` stay as that text, external general entities are not loaded. |
| `CONFIG_EXPAT_XML_DTD` | on | External DTD subsets, parameter entities, conditional sections, and default attributes from an external subset. Requires `CONFIG_EXPAT_XML_GE`. Off: those are not loaded or applied. |

## Documentation

This component does not invent an API. For handlers, parse loops, error codes and feature details, use the upstream manuals:

- [libexpat reference manual](https://libexpat.github.io/doc/)
- [libexpat repository](https://github.com/libexpat/libexpat)
