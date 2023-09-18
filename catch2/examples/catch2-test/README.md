# Catch2 example

This example should help you get started with Catch2 test framework.

## Using the example

To run the example on an ESP32, build and flash the project as usual:

```bash
idf.py set-target esp32
idf.py build flash monitor
```

The example can also be used on Linux host:
```bash
idf.py --preview set-target linux
idf.py build monitor
```

## Example structure

- [main/idf_component.yml](main/idf_component.yml) adds a dependency on `espressif/catch2` component.
- [main/CMakeLists.txt](main/CMakeLists.txt) specifies the source files and registers the `main` component with `WHOLE_ARCHIVE` option enabled.
- [main/test_main.cpp](main/test_main.cpp) implements the application entry point which calls the test runner.
- [main/test_cases.cpp](main/test_cases.cpp) implements one trivial test case.
- [sdkconfig.defaults](sdkconfig.defaults) sets the options required to run the example: enables C++ exceptions and increases the size of the `main` task stack.

## Expected output

```
Randomness seeded to: 3499211612
===============================================================================
All tests passed (1 assertion in 1 test case)

Test passed.
```
