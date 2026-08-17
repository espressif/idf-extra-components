# FreeType Color Emoji Example

This example demonstrates FreeType decoding PNG bitmap glyph data with the ESP-IDF `libpng` component. It loads a bundled CBDT/CBLC font containing only U+1F642 (🙂), then loads that glyph with `FT_LOAD_COLOR`.

The example verifies that FreeType produces a `FT_PIXEL_MODE_BGRA` bitmap and logs its dimensions and FNV-1a hash. A BGRA bitmap confirms that the embedded PNG data was decoded; a grayscale bitmap would indicate that color-glyph decoding was not used.

## Font asset

`main/assets/NotoColorEmoji-smile.ttf` is a 3.7 KiB subset generated from [Noto Color Emoji](https://github.com/googlefonts/noto-emoji) commit [`8998f5dd683424a73e2314a8c1f1e359c19e8742`](https://github.com/googlefonts/noto-emoji/commit/8998f5dd683424a73e2314a8c1f1e359c19e8742) with:

```bash
pyftsubset NotoColorEmoji.ttf --unicodes=U+1F642 \
    --output-file=NotoColorEmoji-smile.ttf
```

The source font is licensed under SIL Open Font License 1.1; a copy is included at `main/assets/LICENSE-OFL.txt`.

## Building and running

Run the application as usual for an ESP-IDF project. For example, for ESP32-P4:

```bash
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

The output includes lines similar to:

```text
I (...) color-emoji: Loaded U+1F642 as 136x128 BGRA bitmap
I (...) color-emoji: FreeType decoded embedded PNG data using libpng; bitmap FNV-1a: ...
```
