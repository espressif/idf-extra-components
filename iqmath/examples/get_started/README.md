# IQMath Get Started

This example demonstrates how to use the [IQMath](https://components.espressif.com/component/espressif/iqmath) library.

## How to Use Example

### Hardware Required

* A development board with Espressif SoC
* A USB cable for Power supply and programming

### Configure the Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Build and Flash

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```text
I (308) main_task: Started on CPU0
I (308) main_task: Calling app_main()
I (308) example: IQMath test passed
I (318) main_task: Returned from app_main()
```
