# Progressive JPEG decoding

This example decodes a **progressive** JPEG the way an application does when the image arrives over a slow link: a fragment at a time, with a complete (coarse, then sharper) picture on the console after every scan.

A progressive JPEG is stored as a series of scans. The first scan carries only DC coefficients, so after a couple of hundred bytes the decoder can already render a full, blocky picture; every following scan sharpens it.

Two libjpeg features make that usable:

* A **data source that can suspend**. When no new data has arrived, `fill_input_buffer()` returns `FALSE` and libjpeg hands control back to the application at a safe restart point. Repeating the same call once the next fragment is there continues where it stopped.
* **Buffered-image mode** (`cinfo.buffered_image = TRUE`). This is what lets you *display* those intermediate pictures. Decoding a progressive file always needs a full-image coefficient buffer (~3 bytes/pixel for 4:2:0) whether or not this mode is on — without it, `jpeg_start_decompress()` swallows every scan and you only get the final image. Buffered-image does **not** add a second framebuffer; it only lets `jpeg_start_output()` render after each scan.

The bundled file is **64×32**, so that coefficient buffer is about 6 KB. The whole picture is printed as ASCII, one character per pixel. No display, no LCD, no extra component.

## Usage

The subsections below give only absolutely necessary information. For full steps to configure ESP-IDF and use it to build and run projects, see [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started).

### Hardware Required

* An Espressif development board based on a chip listed in the supported targets table
* A USB cable for power supply and serial communication

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

To exit the serial monitor, use `Ctrl` + `]`.

## Example Output

Each scan prints how much of the file had arrived, the whole 64×32 picture as it looked then, and its *detail* score (the average difference between neighbouring pixels). Pass 1 is 8×8 blocks; pass 3 has resolved the clouds, the hills and the "ESP32" lettering.

```
Progressive JPEG decoding, one chunk at a time

Progressive image, 64x32, 3 components, 1131 bytes
releasing 64 bytes at a time

pass 1, 256 of 1131 bytes received
----===++******+++==---------::::::::-----====++++++************
----===++******+++==----------------------====++++++************
----===+++****++++==----------------------====++++++************
... (32 rows in total)
^ scan 1 of 2, detail 2

pass 3, 576 of 1131 bytes received
:::::.-:::::::::.-:::::::::::::::::::::::::::::::.::::-:--:::::.
::::::::::.:::::::::::::::::::::::::::::::.:::-:::--:-::::-:--::
:::::-:-:-*%%%*--:-:-::-:-:--::::-:::::::-*#+::::----=+*#*+-::--
-----:::=%%%%%%%-:-----::-:----------::-*%%%%#--=--=#%%%%%%%*---
++++#: :..:++..:.:-+- :::.=*-:=:.=*+....:%%%%%%:::-=++++++++++++
++++*:.=+=-+- -====+-.=#+ :+**+:.+=-:-*..#%%%#+-:::::-=+*+++++++
... (32 rows in total)
^ scan 3 of 3, detail 12

decoded 3 scans from 1131 bytes, 18 suspensions
done
```

Three things are worth reading off that log:

* The first picture costs **256 bytes**. It is blocky, but it is the *whole* image, while the rest of the file is still on the wire.
* The `detail` score climbs from 2 to 12: the picture gets sharper, not more complete.
* The source said "nothing yet" many times. Every one of those suspensions was a libjpeg call returning to `app_main()`, which handed over the next 64 bytes and repeated it.

## Example Breakdown

[`progressive_jpeg_example_main.c`](main/progressive_jpeg_example_main.c) has three parts.

### 1. The suspending data source

`chunk_source` owns the whole file and remembers how much of it the application has released. `fill_input_chunk()` gives the decoder what is there and returns `FALSE` when there is nothing new:

```c
if (src->offset < src->released) {
    src->pub.next_input_byte = src->data + src->offset;
    src->pub.bytes_in_buffer = src->released - src->offset;
    src->offset = src->released;
    return TRUE;
}
src->suspends++;
return FALSE;   /* nothing right now: repeat the call once data arrives */
```

`skip_input_data()` may not suspend, so `skip_input_chunk()` only gives up what is buffered and records the rest; the next `fill_input_chunk()` drops it.

### 2. Buffered-image mode

The loop is the one from the "Buffered-image mode" chapter of `libjpeg.txt`:

```c
cinfo.buffered_image = TRUE;              /* after jpeg_read_header() */
jpeg_start_decompress(&cinfo);            /* reads no input in this mode */

for (;;) {
    while (!jpeg_start_output(&cinfo, cinfo.input_scan_number)) {
        release_next_chunk(&src);         /* suspended: try again */
    }
    while (!read_and_print_pass(&cinfo, row, &diff_sum, &diff_n)) {
        release_next_chunk(&src);         /* suspended mid-pass */
    }
    while (!jpeg_finish_output(&cinfo)) { /* reads up to the next scan */
        release_next_chunk(&src);
    }
    if (jpeg_input_complete(&cinfo) &&
            cinfo.input_scan_number == cinfo.output_scan_number)
        break;
}
```

Passing `cinfo.input_scan_number` to `jpeg_start_output()` keeps the display in lockstep with the arriving data: each pass renders the scan that has just been received, and no sooner.

Every call here can suspend, so every call is repeated until it succeeds. `read_and_print_pass()` is written the same way — its position comes from `cinfo->output_scanline`, which libjpeg maintains across suspensions.

### 3. Rendering

Each output row is read once, reduced to a brightness value, and printed as one character:

```c
static const char ramp[] = " .:-=+*#%@";
return ramp[luma * (int)(sizeof(ramp) - 2) / 255];
```

This is the part an application replaces with `esp_lcd_panel_draw_bitmap()`; nothing around it changes.

## Notes for your own application

* **`CHUNK_SIZE`** is the simulated packet size. Set it to whatever your receive buffer holds (libjpeg recommends at least a couple of KB), and replace `release_next_chunk()` with "copy the received bytes into the source". Nothing else changes.
* **Progressive decode costs a coefficient buffer**, roughly 3 bytes per pixel for 4:2:0, *with or without* buffered-image mode. Keep the image small, or set `cinfo.mem->max_memory_to_use` if that budget has a limit. A 320×240 progressive frame is ~230 KB; this example's 64×32 frame is ~6 KB.
* **Skip buffered-image for baseline JPEGs.** Test `cinfo.progressive_mode` after `jpeg_read_header()` and fall back to plain `jpeg_start_decompress()` plus one `jpeg_read_scanlines()` pass. That path does not need the full-image coefficient buffer.
* The JPEG ships with the application through `EMBED_FILES` in [`main/CMakeLists.txt`](main/CMakeLists.txt). It is 64×32 with a three-scan script: DC, low-frequency Y, remaining Y.
