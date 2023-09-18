# Catch2 console example

This example shows how to combine Catch2 test framework with ESP-IDF `console` component.

In this example, you can execute test cases from an interactive console on an ESP chip.

## Using the example

To run the example, build and flash the project as usual. For example, with an ESP32 chip:

```bash
idf.py set-target esp32
idf.py build flash monitor
```

In the console, use `test` command to invoke Catch2. `test` accepts the same command line arguments as Catch2 tests on the host:

- `test -h` — prints command line argument reference
- `test --list-tests` — lists all the registered tests
- `test` — runs all the registered tests
- `test <test name|pattern|tags>` — runs specific tests

[See Catch2 documentation](https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md) for the complete command line argument reference.
