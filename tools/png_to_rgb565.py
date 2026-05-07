#!/usr/bin/env python3
"""
png_to_rgb565.py - Convert PNG images to RGB565 C arrays for ESP32 TFT displays.

Usage:
    python png_to_rgb565.py input.png --name SPRITE_NAME --output output.h [--swap] [--width W] [--height H]

Output is a C header file with a PROGMEM uint16_t array suitable for TFT_eSPI pushImage().
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is required. Install with: pip install Pillow", file=sys.stderr)
    sys.exit(1)


def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert_image(img, swap_bytes=False):
    """Convert PIL Image to list of RGB565 values."""
    img = img.convert("RGB")
    pixels = list(img.getdata())
    result = []
    for r, g, b in pixels:
        val = rgb888_to_rgb565(r, g, b)
        if swap_bytes:
            # Swap bytes for big-endian displays (SSD1351)
            val = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF)
        result.append(val)
    return result


def generate_header(name, width, height, data, source_filename):
    """Generate C header file content."""
    lines = []
    lines.append(f"// Auto-generated from {source_filename}")
    lines.append(f"// Size: {width}x{height}, Format: RGB565")
    lines.append(f"// Total pixels: {len(data)}, Bytes: {len(data) * 2}")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("#include <pgmspace.h>")
    lines.append("")
    lines.append(f"const uint16_t {name}_data[] PROGMEM = {{")

    # Write 8 values per line
    for i in range(0, len(data), 8):
        chunk = data[i:i+8]
        hex_vals = ", ".join(f"0x{v:04X}" for v in chunk)
        if i + 8 >= len(data):
            lines.append(f"    {hex_vals}")
        else:
            lines.append(f"    {hex_vals},")

    lines.append("};")
    lines.append("")
    lines.append(f"// SpriteAsset usage: {{ {name}_data, {width}, {height} }}")
    lines.append(f"#define {name}_WIDTH  {width}")
    lines.append(f"#define {name}_HEIGHT {height}")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Convert PNG to RGB565 C array for ESP32 TFT displays"
    )
    parser.add_argument("input", help="Input PNG file path")
    parser.add_argument("--name", help="C variable name (default: derived from filename)")
    parser.add_argument("--output", "-o", help="Output .h file (default: stdout)")
    parser.add_argument("--swap", action="store_true",
                        help="Swap bytes for big-endian displays (SSD1351)")
    parser.add_argument("--width", type=int, help="Resize width (optional)")
    parser.add_argument("--height", type=int, help="Resize height (optional)")

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: File not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    # Load image
    img = Image.open(args.input)

    # Resize if requested
    if args.width or args.height:
        w = args.width or img.width
        h = args.height or img.height
        img = img.resize((w, h), Image.LANCZOS)

    width, height = img.size

    # Derive name from filename if not specified
    if args.name:
        name = args.name
    else:
        base = os.path.splitext(os.path.basename(args.input))[0]
        name = base.upper().replace("-", "_").replace(" ", "_")

    # Convert
    data = convert_image(img, swap_bytes=args.swap)

    # Generate header
    source_name = os.path.basename(args.input)
    header_content = generate_header(name, width, height, data, source_name)

    # Output
    if args.output:
        os.makedirs(os.path.dirname(args.output) if os.path.dirname(args.output) else ".", exist_ok=True)
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(header_content)
        print(f"Generated: {args.output} ({width}x{height}, {len(data)*2} bytes)")
    else:
        print(header_content)


if __name__ == "__main__":
    main()