#!/usr/bin/env python3
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0

"""Build logo/luvie.ico (Windows) and logo/luvie.icns (macOS) from logo/png/*.png.

Run by hand after changing the logo; the two outputs are committed, exactly as the
source PNGs in logo/png/ already are. They are not generated during the build, so
neither CI nor a contributor's machine needs an icon toolchain:

    python3 tools/make-icons.py

Both formats are containers that can hold PNG data verbatim, so no image decoding or
re-encoding happens here and stdlib is all that is required. Writing them directly
also avoids depending on iconutil (macOS-only) or png2icns/icotool (not installed by
default anywhere), which would otherwise make this script unrunnable on most machines.

Regenerate the source PNGs themselves from logo/logo.svg with, e.g.:

    inkscape --export-type=png --export-filename=logo/png/luvie-512.png \\
             -w 512 -h 512 logo/logo.svg
"""

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PNG_DIR = ROOT / "logo" / "png"

# Windows shells since Vista read PNG-compressed entries at every size, and the whole
# icon is an order of magnitude smaller than the equivalent uncompressed DIBs. 256 is
# the largest size the ICO directory can describe (it stores dimensions in one byte,
# with 0 meaning 256), so anything above that is macOS-only.
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]

# Apple's four-character type codes, each naming both a pixel size and a scale factor.
# The retina entries (ic11-ic14) are the same pixel counts as the non-retina ones a
# step up, which is why several sizes appear twice: macOS picks by scale, not by
# resolution, so omitting them makes the icon look soft on a Retina display.
ICNS_TYPES = [
    ("icp4", 16),    # 16x16
    ("icp5", 32),    # 32x32
    ("icp6", 64),    # 64x64
    ("ic07", 128),   # 128x128
    ("ic08", 256),   # 256x256
    ("ic09", 512),   # 512x512
    ("ic10", 1024),  # 512x512@2x
    ("ic11", 32),    # 16x16@2x
    ("ic12", 64),    # 32x32@2x
    ("ic13", 256),   # 128x128@2x
    ("ic14", 512),   # 256x256@2x
]


def load(size):
    """Return the bytes of luvie-<size>.png, or None if that size isn't available."""
    path = PNG_DIR / f"luvie-{size}.png"
    return path.read_bytes() if path.exists() else None


def build_ico(out):
    entries = [(s, load(s)) for s in ICO_SIZES]
    entries = [(s, d) for s, d in entries if d]
    if not entries:
        sys.exit(f"no source PNGs found in {PNG_DIR}")

    # ICONDIR: reserved, type 1 (icon), count. Then one 16-byte ICONDIRENTRY each,
    # so image data starts after the whole directory.
    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = len(header) + 16 * len(entries)

    directory, payload = b"", b""
    for size, data in entries:
        directory += struct.pack(
            "<BBBBHHII",
            size if size < 256 else 0,  # width  (0 means 256)
            size if size < 256 else 0,  # height
            0,                          # palette entries; 0 for truecolour
            0,                          # reserved
            1,                          # colour planes
            32,                         # bits per pixel
            len(data),
            offset,
        )
        payload += data
        offset += len(data)

    out.write_bytes(header + directory + payload)
    print(f"{out.relative_to(ROOT)}: {len(entries)} sizes, {out.stat().st_size} bytes")


def build_icns(out):
    chunks = b""
    used = 0
    for code, size in ICNS_TYPES:
        data = load(size)
        if not data:
            continue
        # Each element is its 4-byte type, then a length covering the header itself.
        chunks += code.encode("ascii") + struct.pack(">I", len(data) + 8) + data
        used += 1
    if not used:
        sys.exit(f"no source PNGs found in {PNG_DIR}")

    out.write_bytes(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)
    print(f"{out.relative_to(ROOT)}: {used} elements, {out.stat().st_size} bytes")


if __name__ == "__main__":
    build_ico(ROOT / "logo" / "luvie.ico")
    build_icns(ROOT / "logo" / "luvie.icns")
