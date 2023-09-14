# Catch2 unit testing library

This component is an ESP-IDF wrapper around [Catch2 unit testing library](https://github.com/catchorg/Catch2).

Tests written using this component can be executed both on real hardware and on the host.

## Using Catch2 in ESP-IDF

### Example

To get started with Catch2 quickly, use the example provided along with this component:

```bash
idf.py create-project-from-example "espressif/catch:catch2-test"
cd catch2-test
idf.py set-target esp32
idf.py build flash monitor
```

The example can also be used on host (Linux):
```bash
idf.py --preview set-target linux
idf.py build monitor
```

### Writing tests

Tests and assertions are written as usual for Catch2, refer to the [official documentation](https://github.com/catchorg/Catch2/tree/devel/docs) for more details. Here is a simple example:

```c++
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test case 1")
{
    REQUIRE(1 == 1);
}
```

To ensure the test cases are linked into the application, use `WHOLE_ARCHIVE` argument when calling `idf_component_register()` in CMake. For example:

```cmake
idf_component_register(SRCS "test_main.cpp"
                            "test_cases.cpp"
                       INCLUDE_DIRS "."
                       WHOLE_ARCHIVE)

```

### Enabling C++ exceptions

Catch2 relies on C++ exceptions for assertion handling. To get most benefits out of Catch2, it is recommended to have exceptions enabled in the project (`CONFIG_COMPILER_CXX_EXCEPTIONS=y`).

### Stack size

Catch2 uses significant amount of stack space — around 8kB, plus the stack space used by the test cases themselves. Therefore it is necessary to increase the stack size of the `main` task using the `CONFIG_ESP_MAIN_TASK_STACK_SIZE` option, or to invoke Catch from a new task with sufficient stack size. It is also recommended to keep `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK` or `CONFIG_ESP_SYSTEM_HW_STACK_GUARD` options enabled to detect stack overflows.

### Invoking the test runner

Catch2 test framework can implement the application entry point (`main(int, char**)`) which calls the test runner. This functionality is typically used via the `CATCH_CONFIG_MAIN` macro. However ESP-IDF applications use `app_main(void)` function as an entry point, so the approach with `CATCH_CONFIG_MAIN` doesn't work.


Instead, invoke Catch2 in your `app_main` as follows:

```c++
#include <catch2/catch_session.hpp>

extern "C" void app_main(void)
{
    // prepare command line arguments to Catch2
    const int argc = 1;
    const char* argv[2] = {"target_test_main", NULL};

    // run the tests
    int result = Catch::Session().run(argc, argv);
    // ... handle the result
```

### Integration with ESP-IDF `console` component

This component provides a function to register an ESP-IDF console command to invoke Catch2 test cases:
```c++
esp_err_t register_catch2(const char* cmd_name);
```

This function registers a command with the specified name (for example, "test") with ESP-IDF `console` component. The command passes all the arguments to Catch2 test runner. This makes it possible to invoke tests from an interactive console running on an ESP chip.

To try this functionality, use `catch2-console` example:

```bash
idf.py create-project-from-example "espressif/catch:catch2-console"
cd catch2-console
idf.py set-target esp32
idf.py build flash monitor
```
