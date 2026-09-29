# 2.0.0

## Features

- Strings are returned decoded: escape sequences, including `\uXXXX`
  surrogate pairs, become UTF-8. `*_get_strlen()` returns the decoded length
  and names are matched decoded, so `"r\u00e9sum\u00e9"` is found by `"résumé"`.
- New accessors: `json_obj_get_double()`, `json_arr_get_double()`,
  `json_obj_get_null()`, `json_arr_get_null()`, and the `json_root_get_*()`
  family for a JSON text that is a single value.
- `json_obj_get_val_tok()` and `json_arr_get_val_tok()` return the token
  of a value so its text can be read in place, without a copy (#814).
- Unity test application, and a host harness under `host_test/` that runs
  JSONTestSuite and reads every valid document back through the public API.

## Fixes

- Invalid documents are rejected as RFC 8259 requires: missing or trailing
  commas, misspelled literals, malformed numbers, trailing content, raw
  control characters in strings and malformed UTF-8.
- Numbers are range checked: a value that does not fit `int`, `int64_t`,
  `float` or `double` fails instead of wrapping.
- A number at the end of the text is no longer read past the given length.
- A string buffer of size 0 is refused instead of written to.

## Breaking changes

- Strings come back decoded. Pair with a json_generator that escapes on
  output (espressif/idf-extra-components#859); mixing an old and a new
  version double-escapes or under-escapes strings containing `"` or `\`.
- The length passed to `json_parse_start()` must be the exact text length:
  a NUL byte inside it is an error. Pass `strlen(buf)` or the received byte
  count, not `sizeof(buf)`.
- `*_get_bool()` accepts only `true` and `false`; `1` and `0` are no longer
  booleans.
- A repeated name returns its first occurrence. A value containing U+0000
  or a lone surrogate escape fails to decode; a name containing U+0000
  cannot be looked up.
- Requires jsmn 1.2.0.

# 1.0.3

- Previous release.
