# 1.2.0

## Features

- Strict mode (`JSMN_STRICT`) now accepts exactly the JSON texts of RFC 8259:
  separators, literal spelling, the number grammar, a single top-level value,
  no raw control characters in strings and well-formed UTF-8 are enforced.
- A lone number, string, `true`, `false` or `null` is accepted as a JSON text.
- Test application with Unity cases for the grammar.

## Fixes

- Strict mode no longer accepts trailing or missing commas, unquoted names,
  numbers such as `01`, `1.`, `.5` or `0x1`, `NaN`/`Infinity`, or content
  after the top-level value.

## Breaking changes

- In strict mode the length passed to `jsmn_parse()` must be the exact text
  length: a NUL byte inside it is an error, not an end-of-text marker. Pass
  `strlen(buf)` or the received byte count, not `sizeof(buf)`.
- `jsmn_parser` has a new `state` field; initialise the parser with
  `jsmn_init()`.

Non-strict mode is unchanged.

# 1.1.0

- Upstream jsmn v1.1.0 with `JSMN_PARENT_LINKS`, `JSMN_STRICT` and
  `JSMN_STATIC` selectable through Kconfig.
