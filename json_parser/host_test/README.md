# json_parser host conformance test

Runs on the development host, no target needed:

```sh
./run.sh            # clones JSONTestSuite on first use
```

Two checks, both compiled with ASan/UBSan:

- `accept.c` — every JSONTestSuite document is fed to `json_parse_start()`;
  the `y_` files must be accepted and the `n_` files rejected.
- `api_walk.c` + `check_values.py` — every valid document is parsed by
  Python, then walked through json_parser's public API (objects by name,
  arrays by index, every scalar type including null) and each value
  compared. A value the C API cannot return (U+0000 in a string, a number
  outside `double`) is expected to fail; a name containing U+0000 cannot be
  looked up and is skipped. A sanitizer report or crash fails the run.
