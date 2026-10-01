# 2.0.0

Same code as 1.3.0, which shipped these breaking changes as a minor
release. Use 2.0.0 instead of 1.3.0.

## Features

- Output is a well-formed RFC 8259 JSON text: `"`, `\` and U+0000..U+001F
  are escaped in string values, element names and long strings.
- String input is validated as UTF-8; invalid input fails with
  `JSON_GEN_ERR_INVALID_UTF8`.
- New `json_gen_obj_set_double()` and `json_gen_arr_set_double()`, written
  with round-trip precision.
- New `json_gen_obj_set_string_len()`, `json_gen_arr_set_string_len()` and
  `json_gen_add_to_long_string_len()`, so a value may contain U+0000.
- New `json_gen_str_start_measure()`: a pass that only counts, so a buffer
  of the exact size can be allocated before the real pass.
- Errors are sticky: the first one is returned by every later call and by
  `json_gen_str_end()`, so checking that call alone is enough.

## Fixes

- `json_gen_*_set_int64()` works with the newlib nano printf
  (`CONFIG_LIBC_NEWLIB_NANO_FORMAT`); it used to write `ld`.
- Large floats are no longer truncated, and a number that does not fit
  fails with `JSON_GEN_ERR_NUM_TRUNC`.
- NaN and infinity are written as `null` instead of the invalid `nan`/`inf`.

## Breaking changes

- Strings are escaped on output. Callers that escaped strings themselves
  before passing them in (for example, turning newlines in a PEM certificate
  or CSR into `\n`) now get them escaped twice: `\n` becomes `\\n` on the
  wire, and the receiver reads a backslash followed by `n`. Remove the
  manual escaping and pass the raw string.
- A receiver that unescaped values by hand must stop doing so when paired
  with json_parser 2.0.0, which returns strings already decoded.
- `json_gen_str_end()` returns a negative `JSON_GEN_ERR_*` code on failure
  instead of always returning the length. Check for `< 0` before using the
  result as a size.
- Strings that are not valid UTF-8 are rejected instead of copied through.

# 1.2.0

- Previous release.
