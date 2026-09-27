# JSON Generator

[![Component Registry](https://components.espressif.com/components/espressif/json_generator/badge.svg)](https://components.espressif.com/components/espressif/json_generator)

A simple JSON (JavasScript Object Notation) generator with flushing capability.
Details of JSON can be found at [http://www.json.org/](http://www.json.org/).
The output is always a well-formed JSON text as defined by
[RFC 8259](https://www.rfc-editor.org/rfc/rfc8259): string values and element
names are escaped as required, and must be valid UTF-8.

# Files
- `src/json_generator.c`: Actual source file for the JSON generator with implementation of all APIS
- `include/json_generator.h`: Header file documenting and exposing all available APIs

# Usage

Include the C and H files in your project's build system and that should be enough.
`json_generator` requires only standard library functions for compilation

# Behaviour

- **Strings** — `"`, `\` and control characters are escaped; everything else,
  including non-ASCII UTF-8, is copied through. Input that is not well-formed
  UTF-8 is rejected with `JSON_GEN_ERR_INVALID_UTF8` and nothing is written for
  it. A multi-byte sequence may be split across `json_gen_add_to_long_string()`
  calls. Values are NUL-terminated C strings; the `*_len()` variants take an
  explicit length, so a value may contain U+0000 (written as `\u0000`).
  Element names are always NUL-terminated and cannot contain U+0000.
- **Numbers** — `json_gen_*_set_float()` writes a fixed number of decimals
  (`JSON_FLOAT_PRECISION`, default 5). `json_gen_*_set_double()` writes the
  fewest significant digits (15 to 17) that parse back to the same value, so
  `0.1` is written as `0.1`, not `0.10000000000000001`. NaN and infinity have
  no JSON representation and are written as `null`.
- **Errors** — every generating call returns `0` or a negative
  `JSON_GEN_ERR_*` code; `json_gen_str_end()` returns the generated size
  (including the terminator) or the negative code. The first error is
  remembered and every later call is a no-op returning it, so checking
  `json_gen_str_end()` alone is enough. After an error the buffer contents
  are unspecified.
- **Sizing** — `json_gen_str_start_measure()` starts a pass that only counts;
  `json_gen_str_end()` then returns the buffer size (including the terminator)
  the same calls will need:

  ```c
  json_gen_str_t jstr;
  json_gen_str_start_measure(&jstr);
  write_document(&jstr);              /* the same json_gen_* calls as below */
  int len = json_gen_str_end(&jstr);
  if (len < 0) { /* handle error */ }
  char *buf = malloc(len);
  json_gen_str_start(&jstr, buf, len, NULL, NULL);
  write_document(&jstr);
  json_gen_str_end(&jstr);
  ```
- **Pre-formatted input** — `json_gen_push_object_str()` and
  `json_gen_push_array_str()` copy their argument verbatim. The caller must
  guarantee it is valid JSON.
