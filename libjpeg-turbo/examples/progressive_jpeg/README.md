# Progressive JPEG decoding

This example decodes a **progressive** JPEG the way an application does when the image arrives over a slow link: a fragment at a time, with a preview on screen long before the transfer is finished.

A progressive JPEG is stored as a series of scans. The first scans carry only the lowest-frequency coefficients, so after a few hundred of the file's 16 KB the decoder can already render a complete, coarse picture; every following scan sharpens it. Two features of libjpeg make that usable:

* A **data source that can suspend**. When no new data has arrived, `fill_input_buffer()` returns `FALSE` and libjpeg hands control back to the application at a safe restart point instead of failing. Repeating the same call once the next fragment is there continues where it stopped.
* **Buffered-image mode** (`cinfo.buffered_image = TRUE`). The decoder keeps every coefficient it has seen, so each scan refines the buffered image and `jpeg_start_output()` can render it as many times as wanted.

Nothing else is needed. No display, no LCD, no extra component: every intermediate image is printed on the console as ASCII art.

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

Each scan prints how much of the file had arrived, the top-left 72x26 pixels of the image as it looked then, and its *detail* score, the average difference between neighbouring pixels. Two of the ten passes are shown here; the first scan is a blur of flat blocks, and later scans resolve the picture inside them.

```
Progressive JPEG decoding, one chunk at a time

Progressive image, 320x240, 3 components, 16476 bytes
releasing 64 bytes at a time

pass 1, 256 of 16476 bytes received
------------------------------------------------------------------------
------------------------------------------------------------------------
------------------------------------------------------------------------
------------------------========-----------=====------------------------
----===---------------============-------==========---------------------
============---------==============---=============---------------------
==============------================================--------------===---
... (26 rows in total)
^ scan 1 of 2, detail 1

pass 10, 11904 of 16476 bytes received
------------------------------------------------------------------------
------------------------------------------------------------------------
------------------------------------------------------------------------
------------------------------------------------------------------------
------*%%%%%%%%%%%%*-------*#%%%%%#*+=-=--*%%%%%%%%%%*+-------==*#%%%%##
------#%%%%%%%%%%%%*-----+%%%%%%%%%%%*=---*%%%%%%%%%%%%#=-----*%%%%%%%%%
----=-#%=:..:......:.:--=%%%%+-:::::==----#%=........::=*=----*%%#+-::::
... (26 rows in total)
^ scan 10 of 10, detail 15

decoded 10 scans from 16476 bytes, 258 suspensions
done
```

Three things are worth reading off that log:

* The first picture costs **256 bytes**. It is useless on its own, but it is there while the remaining 16 KB are still being transferred.
* The `detail` score climbs from 1 to 15 as the scans arrive: the picture gets sharper, not more complete.
* The source said "nothing yet" **258 times**. Every one of those suspensions was a libjpeg call returning to `app_main()`, which promptly handed over the next 64 bytes and repeated it.

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
if (src->eof) {
    /* Whole file released and still no EOI: the stream was truncated,
     * so hand over a synthetic EOI marker as libjpeg.txt recommends. */
    WARNMS(cinfo, JWRN_JPEG_EOF);
    ...
    return TRUE;
}
s_suspends++;
return FALSE;   /* nothing right now: repeat the call once data arrives */
```

`skip_input_data()` may not suspend, so `skip_input_chunk()` only gives up what is buffered and records the rest; the next `fill_input_chunk()` drops it. This is the documented behaviour for a source that has to cope with markers.

### 2. Buffered-image mode

The loop is the one from the "Buffered-image mode" chapter of `libjpeg.txt`:

```c
cinfo.buffered_image = TRUE;              /* after jpeg_read_header() */
jpeg_start_decompress(&cinfo);            /* reads no input in this mode */

for (;;) {
    while (!jpeg_start_output(&cinfo, cinfo.input_scan_number)) {
        release_next_chunk(&s_src);       /* suspended: try again */
    }
    while (!read_pass_into_crop(&cinfo, row)) {
        release_next_chunk(&s_src);       /* suspended mid-pass */
    }
    print_crop();
    while (!jpeg_finish_output(&cinfo)) { /* reads up to the next scan */
        release_next_chunk(&s_src);
    }
    if (jpeg_input_complete(&cinfo) &&
            cinfo.input_scan_number == cinfo.output_scan_number)
        break;
}
```

Passing `cinfo.input_scan_number` to `jpeg_start_output()` is what keeps the display in lockstep with the arriving data: each pass renders the scan that has just been received, and no sooner.

Every call here can suspend, so every call is repeated until it succeeds. `read_pass_into_crop()` is written the same way — its whole state comes from `cinfo->output_scanline`, which libjpeg maintains across suspensions, so a single output pass can be interrupted dozens of times and still produce one complete picture.

### 3. Rendering

Each output row is read once and the brightness of the pixels inside the 72x26 crop is copied out, one character per pixel:

```c
static const char ramp[] = " .:-=+*#%@";
return ramp[luma * (int)(sizeof(ramp) - 2) / 256];
```

Rows outside the crop still have to be read — libjpeg hands out scanlines in order — they are just not printed. This is the part an application replaces with `esp_lcd_panel_draw_bitmap()`; nothing around it changes.

## Notes for your own application

* **`CHUNK_SIZE`** is the simulated packet size. Set it to whatever your receive buffer holds, and replace `release_next_chunk()` with "copy the received bytes into the source's ring buffer". Nothing else changes.
* **Buffered-image mode costs memory**: it holds a coefficient buffer for the whole image, roughly 6 bytes per pixel for 4:2:0. Set `cinfo.mem->max_memory_to_use` if that budget has a limit.
* **Not every JPEG is progressive.** Test `cinfo.progressive_mode` after `jpeg_read_header()` and fall back to plain `jpeg_start_decompress()` plus one `jpeg_read_scanlines()` pass for baseline files.
* The JPEG ships with the application through `EMBED_FILES` in [`main/CMakeLists.txt`](main/CMakeLists.txt). It is 320x240 with the usual ten-scan script.
