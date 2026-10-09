# JSON Parser

[![Component Registry](https://components.espressif.com/components/espressif/json_parser/badge.svg)](https://components.espressif.com/components/espressif/json_parser)

This is a simple, light weight JSON parser built on top of [jsmn](https://github.com/zserge/jsmn).
It accepts exactly the JSON texts of RFC 8259 and returns string values decoded.

Files

- `src/json_parser.c`: Source file which has all the logic for implementing the APIs built on top of JSMN
- `include/json_parser.h`: Header file that exposes all APIs

# Behaviour

- **Documents** — `json_parse_start()` accepts any RFC 8259 JSON text and
  nothing else. A text that is a single string, number, `true`, `false` or
  `null` is read with the `json_root_get_*()` functions.
- **Strings** — returned decoded: escapes including `\uXXXX` surrogate pairs
  become UTF-8. `*_get_strlen()` returns the decoded length in bytes, without
  the terminator. Names are matched decoded. A value containing U+0000
  or a lone surrogate escape cannot be decoded into a C string, and the
  accessor fails; a name containing U+0000 cannot be looked up.
- **Numbers** — `*_get_int()`, `*_get_int64()`, `*_get_float()` and
  `*_get_double()` fail when the value does not fit the type, and an integer
  accessor fails on a number with a fraction or exponent.
- **Booleans and null** — `*_get_bool()` accepts only `true` and `false`;
  `*_get_null()` succeeds when the value is `null`.
- **Repeated names** — the first occurrence is used.
- **Raw text** — `*_get_object_str()` / `*_get_array_str()` copy the original
  text of a nested value, unmodified. `json_obj_get_val_tok()` /
  `json_arr_get_val_tok()` return the token of a value so its text can be
  read in place, without a copy; a string token with no backslash is already
  its decoded value.

# Tests

`test_apps/` is a Unity suite that runs on target. `host_test/` runs
[JSONTestSuite](https://github.com/nst/JSONTestSuite) on the host: every
document is checked for accept/reject, and every valid document is walked
through the public API and compared value by value against a strict parser.

