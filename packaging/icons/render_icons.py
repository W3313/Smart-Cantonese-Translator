#!/usr/bin/env python3
"""Regenerate the raster app icons from the SVG sources.

    pip install cairosvg pillow
    python packaging/icons/render_icons.py

Inputs:
    resources/icons/app.svg          main artwork (32 px and larger)
    packaging/icons/app-small.svg    simplified artwork for 16 and 24 px
Outputs:
    resources/icons/app.png          256x256 PNG (window icon / README)
    resources/icons/app.ico          16, 24, 32, 48, 64, 128, 256 px (exe + installer icon)

The ICO uses 32-bit BMP entries for the small sizes (best compatibility with
rc.exe, Explorer and Inno Setup) and a PNG-compressed 256 px entry.
"""

import io
import struct
from pathlib import Path

import cairosvg
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
MAIN_SVG = ROOT / "resources" / "icons" / "app.svg"
SMALL_SVG = ROOT / "packaging" / "icons" / "app-small.svg"
OUT_PNG = ROOT / "resources" / "icons" / "app.png"
OUT_ICO = ROOT / "resources" / "icons" / "app.ico"

ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)


def render(svg: Path, size: int) -> Image.Image:
    png = cairosvg.svg2png(url=str(svg), output_width=size, output_height=size)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def bmp_entry(img: Image.Image) -> bytes:
    """32-bit BGRA DIB with an AND mask, as stored inside .ico files."""
    w, h = img.size
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = bytearray()
    for y in range(h - 1, -1, -1):  # bottom-up
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            pixels += bytes((b, g, r, a))
    row_bytes = ((w + 31) // 32) * 4
    mask = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(row_bytes)
        for x in range(w):
            if img.getpixel((x, y))[3] == 0:
                row[x // 8] |= 0x80 >> (x % 8)
        mask += row
    return header + bytes(pixels) + bytes(mask)


def png_entry(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def write_ico(images: list[Image.Image], path: Path) -> None:
    blobs = [png_entry(im) if im.size[0] >= 256 else bmp_entry(im) for im in images]
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    offset = 6 + 16 * len(images)
    for im, blob in zip(images, blobs):
        w, h = im.size
        out += struct.pack("<BBBBHHII", w % 256, h % 256, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    for blob in blobs:
        out += blob
    path.write_bytes(bytes(out))


def main() -> None:
    images = [render(SMALL_SVG if s <= 24 else MAIN_SVG, s) for s in ICO_SIZES]
    write_ico(images, OUT_ICO)
    render(MAIN_SVG, 256).save(OUT_PNG, format="PNG", optimize=True)
    print(f"wrote {OUT_ICO.relative_to(ROOT)} and {OUT_PNG.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
