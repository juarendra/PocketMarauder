#!/usr/bin/env python3
"""Generate firmware/esp32_marauder/pocket_logo.h from a source image.

Usage: python tools/make_logo.py <source_image.png>
Output: 128x64 1bpp XBM-style byte array (MSB-first, lit=black pixel) for SSD1306.
"""
import sys
from PIL import Image, ImageOps, ImageEnhance, ImageFilter

W, H = 128, 64
OUT = "firmware/esp32_marauder/pocket_logo.h"

def main(src):
    base = Image.open(src).convert("RGBA")
    white = Image.new("RGBA", base.size, (255, 255, 255, 255))
    white.paste(base, (0, 0), base)
    im = white.convert("L")
    # crop to subject (tuned for the 433x577 Frenchie source; adjust if source changes)
    if im.size == (433, 577):
        im = im.crop((40, 20, 393, 400))
    im = ImageOps.autocontrast(im, cutoff=2)
    im = ImageEnhance.Contrast(im).enhance(1.4)
    im = im.filter(ImageFilter.SHARPEN)
    r = min(W / im.width, H / im.height)
    nw, nh = int(im.width * r), int(im.height * r)
    im2 = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("L", (W, H), 255)
    canvas.paste(im2, ((W - nw) // 2, (H - nh) // 2))
    bw = canvas.point(lambda p: 0 if p < 125 else 255, "1")
    px = bw.load()
    data = []
    for y in range(H):
        for xb in range(0, W, 8):
            b = 0
            for bit in range(8):
                if px[xb + bit, y] == 0:
                    b |= 1 << (7 - bit)
            data.append(b)
    lines = ["  " + ", ".join(f"0x{v:02X}" for v in data[i:i+16]) + ","
             for i in range(0, len(data), 16)]
    out = ("// Auto-generated boot logo for PocketMarauder (128x64, 1bpp, MSB-first, lit=black pixel)\n"
           "// Source: French Bulldog photo. Regenerate via tools/make_logo.py\n"
           "#pragma once\n#define POCKET_LOGO_W 128\n#define POCKET_LOGO_H 64\n"
           "static const unsigned char PROGMEM pocket_logo_bits[1024] = {\n"
           + "\n".join(lines) + "\n};\n")
    with open(OUT, "w") as f:
        f.write(out)
    assert len(data) == 1024, f"expected 1024 bytes, got {len(data)}"
    print(f"wrote {OUT} ({len(data)} bytes)")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "docs/logo_src.png")
