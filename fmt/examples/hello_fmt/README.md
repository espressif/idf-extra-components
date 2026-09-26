# `fmt` example: a tour of the formatting library

This example prints a short tour of the [fmt](https://fmt.dev/latest/index.html) formatting library on the serial console. It is a starting point for replacing `printf` or C++ iostreams with type-safe format strings in an ESP-IDF application.

## Usage

The subsections below give only absolutely necessary information. For full steps to configure ESP-IDF and use it to build and run projects, see [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started).

### Hardware Required

* Any Espressif development board
* A USB cable for power supply and serial communication

### Build and Flash

Execute the following command to build the project, flash it to your development board, and run the monitor tool to view the serial output:

```bash
idf.py set-target <target>
idf.py build flash monitor
```

This command can be reduced to `idf.py flash monitor`.

To exit the serial monitor, use `Ctrl` + `]`.

## Example Output

If you see the following console output, your example should be running correctly. Timestamps, ANSI color codes, and the chip name / CPU frequency may differ.

```
--- basic formatting ---
Hello, fmt!
The answer is 42 and pi is approximately 3.142
|left      |     right|  center  |
Manual indexing: braces again braces
Curly braces are escaped like this: "{} -> {}"
Binary: 0b11111101010, Hex: 0x7ea, Octal: 03752
--- fmt::format ---
formatted string: 'Hello, ESP-IDF #2!' (length 18)
--- fmt::format_to ---
appended into std::string: 'hello world'
written into a fixed buffer: 'idf v6.0' (8 chars)
--- named arguments ---
fmt runs on esp32s3 at 160 MHz
FMT_STRING is checked at compile time: ok
--- ranges and tuples ---
vector: [1, 2, 3, 4, 5]
map: {"alice": 90, "bob": 85}
pair: (1, 2), tuple: ("fmt", 12, 2)
range with spec: [0x0a, 0x0b, 0x0c]
--- styled text ---
This line is bold steel blue
White text on a crimson background
Everything is fine: true
--- chrono ---
current time: 2026-09-11 07:37:35.799369439
one and a half hours: 01:30:00
sub-second precision: 1.500s
--- dynamic_format_arg_store ---
built at run time: dynamic in 2026
--- printf compatibility ---
printf-style: fmt::printf has 2 args
fmt::sprintf returns a string: '03.14'
--- done ---
```

## Reference

- [fmt documentation](https://fmt.dev/latest/index.html)
- [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started)

For any technical queries, please open an [issue](https://github.com/espressif/idf-extra-components/issues) on GitHub.
