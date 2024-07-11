# `fmt` basic example

This is a basic example of using `fmt` library in an ESP-IDF project. It includes `fmt` and prints `Hello, fmt!` on the console.

The example runs on any ESP development board. To build and run the example, follow the same steps as for any other ESP-IDF project. For example, for ESP32-C3:

```bash
idf.py set-target esp32c3
idf.py flash monitor
```

The example should print the following:

```
I (6074) main_task: Calling app_main()
Hello, fmt!
I (6084) main_task: Returned from app_main()
```
