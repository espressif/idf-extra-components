/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <array>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "sdkconfig.h"

#include <fmt/args.h>      // dynamic argument lists (fmt::dynamic_format_arg_store)
#include <fmt/chrono.h>    // date and time formatting
#include <fmt/color.h>     // terminal colors and text styles
#include <fmt/core.h>      // the core API: fmt::print, fmt::format, fmt::format_to
#include <fmt/printf.h>    // the printf-compatible API, for code ported from C
#include <fmt/ranges.h>    // formatting of ranges, maps, tuples and pairs
#include <fmt/std.h>       // formatters for std::filesystem::path, std::thread::id, ...

extern "C" void app_main()
{
    // 1. The classic, type-safe replacement of printf
    fmt::print("--- basic formatting ---\n");
    fmt::print("Hello, fmt!\n");
    fmt::print("The answer is {} and pi is approximately {:.3f}\n", 42, 3.141592653589793);

    // Positional arguments, fill characters and alignment, all in one format string
    fmt::print("|{:<10}|{:>10}|{:^10}|\n", "left", "right", "center");
    fmt::print("Manual indexing: {0} {1} {0}\n", "braces", "again");
    fmt::print("Curly braces are escaped like this: \"{{}} -> {{}}\"\n");
    fmt::print("Binary: {:#b}, Hex: {:#x}, Octal: {:#o}\n", 2026, 2026, 2026);

    // 2. fmt::format returns an std::string instead of writing to the console
    fmt::print("--- fmt::format ---\n");
    const std::string greeting = fmt::format("Hello, {} #{}!", "ESP-IDF", 2);
    fmt::print("formatted string: '{}' (length {})\n", greeting, greeting.size());

    // 3. fmt::format_to appends to any output iterator; a back_inserter or a
    //    fixed-size buffer is a natural target on an embedded device.
    fmt::print("--- fmt::format_to ---\n");
    std::string out;
    fmt::format_to(std::back_inserter(out), "{} {}", "hello", "world");
    fmt::print("appended into std::string: '{}'\n", out);

    char buffer[32] = {};
    auto end = fmt::format_to_n(buffer, sizeof(buffer) - 1, "idf v{}", "6.0");
    fmt::print("written into a fixed buffer: '{}' ({} chars)\n", buffer, end.size);

    // 4. Named arguments: formatting a string coming from a translation or a
    //    config file without relying on the argument order.
    fmt::print("--- named arguments ---\n");
#ifdef CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
    fmt::print("{name} runs on {chip} at {clock} MHz\n", fmt::arg("name", "fmt"),
               fmt::arg("chip", CONFIG_IDF_TARGET),
               fmt::arg("clock", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ));
#else
    fmt::print("{name} runs on {chip}\n", fmt::arg("name", "fmt"),
               fmt::arg("chip", CONFIG_IDF_TARGET));
#endif

    // FMT_STRING makes the format string verifiable even on pre-C++20 toolchains:
    // a mistake here is a compile error, not a run-time one.
    fmt::print(FMT_STRING("FMT_STRING is checked at compile time: {}\n"), "ok");

    // 5. Ranges, tuples and maps are printed out of the box
    fmt::print("--- ranges and tuples ---\n");
    std::vector<int> values{1, 2, 3, 4, 5};
    std::map<std::string, int> scores{{"alice", 90}, {"bob", 85}};
    auto point = std::make_pair(1, 2);
    auto record = std::make_tuple("fmt", 12, 2.0);

    fmt::print("vector: {}\n", values);
    fmt::print("map: {}\n", scores);
    fmt::print("pair: {}, tuple: {}\n", point, record);
    fmt::print("range with spec: {::#04x}\n", std::array<uint8_t, 3> {0x0a, 0x0b, 0x0c});

    // 6. Colors and text emphasis, using the ANSI escape codes.
    //    fmt::println applies the reset (\x1b[0m) before the newline; putting
    //    '\n' inside fmt::print would leave the background painted to EOL.
    fmt::print("--- styled text ---\n");
    fmt::println(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "This line is bold steel blue");
    fmt::println(fg(fmt::color::white) | bg(fmt::color::crimson), "White text on a crimson background");
    const auto ok_style = fg(fmt::terminal_color::green) | fmt::emphasis::bold;
    fmt::println(ok_style, "Everything is fine: {}", true);

    // 7. Date and time formatting
    fmt::print("--- chrono ---\n");
    using namespace std::chrono;
    const auto now = system_clock::now();
    fmt::print("current time: {:%Y-%m-%d %H:%M:%S}\n", now);
    fmt::print("one and a half hours: {:%H:%M:%S}\n", seconds{5400});
    fmt::print("sub-second precision: {:.3}\n", duration<double>(milliseconds{1500}));

    // 8. Dynamic argument store: build the argument list at run time, which is
    //    handy when the number of arguments is not known at compile time.
    fmt::print("--- dynamic_format_arg_store ---\n");
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(std::string("dynamic"));
    store.push_back(2026);
    fmt::print("built at run time: {}\n", fmt::vformat("{} in {}", store));

    // 9. The printf family, for code being ported from C
    fmt::print("--- printf compatibility ---\n");
    fmt::printf("printf-style: %s has %d args\n", "fmt::printf", 2);
    fmt::print("fmt::sprintf returns a string: '{}'\n", fmt::sprintf("%05.2f", 3.14159));

    fmt::print("--- done ---\n");
}
