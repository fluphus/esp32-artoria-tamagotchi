# Generated Assets

This directory holds auto-generated RGB565 C arrays for use as sprite/animation assets.

## How to convert PNG to RGB565 C array

### Prerequisites
- Python 3.8+
- Pillow (`pip install Pillow`)

### Using the conversion script

```bash
python tools/png_to_rgb565.py input.png --name MY_SPRITE --output src/display/generated_assets/my_sprite.h
```

### Options
- `--name` : C variable name for the array (default: derived from filename)
- `--output` : Output .h file path (default: stdout)
- `--swap` : Swap bytes for big-endian displays (SSD1351 needs this)
- `--width` / `--height` : Resize before conversion (optional)

### Output format

The script generates a header file like:

```c
// Auto-generated from input.png
// Size: 64x64, Format: RGB565 (big-endian)
#pragma once
#include <stdint.h>

const uint16_t MY_SPRITE_data[] PROGMEM = {
    0xFFFF, 0x0000, ...
};

// SpriteAsset-compatible: { MY_SPRITE_data, 64, 64 }
```

### Integrating with display_assets.cpp

1. Generate the .h file into `src/display/generated_assets/`
2. Include it in `display_assets.cpp`
3. Replace the `{ nullptr, 0, 0 }` placeholder with `{ MY_SPRITE_data, W, H }`

### Recommended sprite sizes (128x128 screen)
- Pet form sprites: 64x64
- Food sprites: 24x24
- Special food sprites: 24x24
- Animation frames: 64x64

### Color format
- RGB565: 16-bit color, 5 bits red, 6 bits green, 5 bits blue
- SSD1351 uses big-endian byte order (MSB first) - use `--swap` flag