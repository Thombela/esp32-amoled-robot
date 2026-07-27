#!/usr/bin/env python3
"""Convert a photo (JPEG/PNG/etc, via Pillow) to a flat opaque RGB565 LVGL
array for a full-screen background. Companion to tools/png_to_lvgl.js, which
is PNG+alpha-only and always emits RGB565A8 (color+alpha planes) — wrong for
an opaque photo, since it'd waste a whole alpha plane on all-0xFF bytes.
"""
import argparse
from PIL import Image


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert(path, size):
    img = Image.open(path).convert("RGB")
    if size:
        img = img.resize(size, Image.LANCZOS)
    w, h = img.size
    return w, h, [rgb565(r, g, b) for (r, g, b) in img.getdata()]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("symbol")
    ap.add_argument("macro_prefix")
    ap.add_argument("--size", help="WxH, e.g. 480x480")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    size = None
    if args.size:
        w, h = args.size.lower().split("x")
        size = (int(w), int(h))

    w, h, pixels = convert(args.input, size)
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define {args.macro_prefix}_W {w}",
        f"#define {args.macro_prefix}_H {h}",
        f"// Opaque RGB565 (little-endian), no alpha -- {w*h} pixels",
        f"static const uint16_t {args.symbol}[{w*h}] = {{",
    ]
    for i in range(0, len(pixels), 12):
        lines.append("    " + ", ".join(f"0x{p:04X}" for p in pixels[i:i + 12]) + ",")
    lines.append("};")
    with open(args.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Wrote {args.out}: {w}x{h} ({args.symbol})")


if __name__ == "__main__":
    main()
