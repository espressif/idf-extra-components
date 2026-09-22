# libjpeg-turbo Decode Example

This example shows how to decode JPEG images with the libjpeg-turbo component on ESP-IDF.

Two JPEG files are embedded in the firmware (`image.jpg` at 320×240 and `image32x32.jpg` at 32×32). The application feeds each buffer to the upstream **libjpeg C API** (`jpeglib.h`) through `jpeg_mem_src()`, decompresses the scanlines, and prints progress on the serial console. The smaller image is also rendered as ASCII art so you can see that the pixels came out correctly — no display is required.

Use this as a starting point for camera snapshots, asset decoding, or any in-memory JPEG pipeline. The same `jpeg_create_decompress()` / `jpeg_read_header()` / `jpeg_read_scanlines()` sequence is what you would call on a framebuffer or a file you have already loaded into RAM.

## Usage

The subsections below give only absolutely necessary information. For full steps to configure ESP-IDF and use it to build and run projects, see [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started).

### Hardware Required

* An Espressif development board based on a chip listed in the supported targets table
* A USB cable for power supply and serial communication

No camera, LCD, or other external hardware is required.

### Set Chip Target

For example, to set esp32 as the chip target, run:

```
idf.py set-target esp32
```

### Build and Flash

Execute the following command to build the project, flash it to your development board, and run the monitor tool to view the serial output:

```
idf.py build flash monitor
```

This command can be reduced to `idf.py flash monitor`.

If the above command fails, check the log on the serial monitor which usually provides information on the possible cause of the issue.

To exit the serial monitor, use `Ctrl` + `]`.

## Example Output

If you see the following console output, your example should be running correctly. The `time =` value depends on the chip and compiler optimization. Timestamps and other IDF log lines may differ.

```
app_main started
P6
320 240
255
jpeg_start_decompress
jpeg_finish_decompress, time = 2300000
jpeg_destroy_decompress
P6
32 32
255
Decoded image 32x32:
################################
################################
################################
################################
################################
################################
################################
################################
################################
################################
####     .###+   ####    +######
#### #######+.###+###.### .#####
#### #######.########.#### #####
#### ####### +#######.#### #####
#### ########  ######.#### #####
####     #####   ####.### +#####
#### ###########  ###    +######
#### ############ ###.##########
#### ############ ###.##########
#### #######.###+ ###.##########
####     .###   .####.##########
################################
################################
################################
################################
################################
################################
################################
################################
################################
################################
################################
done
```

The first `P6` / `320 240` / `255` block is a PPM header for `image.jpg`. The second block is the 32×32 image printed as `#` `+` `.` and spaces.

## Example Breakdown

[`hello_jpeg_example_main.c`](main/hello_jpeg_example_main.c) walks through the libjpeg decompress path twice:

1. Create a `jpeg_decompress_struct` and attach the embedded JPEG with `jpeg_mem_src()`.
2. Read the header, print a PPM header, then `jpeg_start_decompress()` / `jpeg_read_scanlines()`.
3. Repeat for `image32x32.jpg`, reducing each row to ASCII so the pixels are visible on the serial console.

The JPEG files are compiled into the application with `EMBED_FILES` in [`main/CMakeLists.txt`](main/CMakeLists.txt).
