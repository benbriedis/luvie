#!/usr/bin/env python3
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0

"""Build src/fonts/LuvieSans-Regular.ttf and -Bold.ttf, the UI font compiled into Luvie.

Run by hand after changing what goes into the font; the outputs are committed, as the
icons are, so neither CI nor a contributor's machine needs fontTools:

    pip install fonttools
    python3 tools/make-ui-font.py

The font is Noto Sans cut down to Basic Latin, Latin-1, Latin Extended-A, punctuation
and currency. Anything else -- Greek, Cyrillic, Korean... in a user's track name, or a
symbol such as the tempo ramp's arrow -- comes from the system's fonts by fallback
(Pango on Linux), so it is not worth carrying here. The UI's own symbols (menu arrows,
ticks, disclosure triangles, sharps and flats) are drawn from SVG artwork, not text;
see src/modern/svgGlyph.hpp.

The family is renamed "Luvie Sans". The OFL would allow keeping the name (Noto reserves
none), but a font called "Noto Sans" that is missing most of Noto Sans would shadow, or
be shadowed by, the real one installed on most Linux systems, and which wins is up to
fontconfig. A name of its own makes the font Luvie asks for unambiguous.

Sources are fetched from the Noto project at a pinned commit and checked against the
hashes below; bump NOTO_COMMIT and the hashes together to move to newer Noto.
"""

import hashlib
import sys
import urllib.request
from pathlib import Path

from fontTools.subset import Options, Subsetter
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "src" / "fonts"
CACHE = ROOT / "build" / "ui-font-sources"

NOTO_COMMIT = "c6f674cf56243b4c0b650134a9020e047eab702e"
NOTO_URL = ("https://raw.githubusercontent.com/notofonts/notofonts.github.io/"
            f"{NOTO_COMMIT}/fonts/{{}}")

SOURCES = {
    "NotoSans-Regular.ttf": "NotoSans/hinted/ttf/NotoSans-Regular.ttf",
    "NotoSans-Bold.ttf":    "NotoSans/hinted/ttf/NotoSans-Bold.ttf",
}
# A mismatch means the files at the pinned commit are not the ones these were taken from.
SHA256 = {
    "NotoSans-Regular.ttf": "478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823",
    "NotoSans-Bold.ttf":    "1df075a380fc7cb898acf64c1f7b3b4dd780de3caa860178bf929de35817a913",
}

TEXT = (
    list(range(0x0020, 0x007F)) +   # Basic Latin
    list(range(0x00A0, 0x0100)) +   # Latin-1 Supplement
    list(range(0x0100, 0x0180)) +   # Latin Extended-A
    list(range(0x2000, 0x2070)) +   # General Punctuation (… — – ‘ ’ “ ” • …)
    list(range(0x20A0, 0x20C1)) +   # Currency Symbols
    [0x2122, 0x2212, 0xFFFD]        # ™, minus sign, replacement character
)

STYLES = ("Regular", "Bold")


def fetch(name):
    CACHE.mkdir(parents=True, exist_ok=True)
    path = CACHE / name
    if not path.exists():
        print(f"fetching {name}")
        urllib.request.urlretrieve(NOTO_URL.format(SOURCES[name]), path)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    want = SHA256.get(name)
    if want is None:
        print(f'    "{name}": "{digest}",   # not yet pinned')
    elif digest != want:
        sys.exit(f"{name}: sha256 {digest} does not match the pinned {want}")
    return path


def subset(path, unicodes):
    font = TTFont(path)
    opts = Options()
    opts.name_IDs = ["*"]
    opts.name_languages = ["*"]
    opts.notdef_outline = True
    # fontTools' default feature set: kerning, ligatures, mark positioning and the
    # like. "*" would also keep small caps, old-style figures and stylistic sets,
    # which roughly doubles the glyph count for features nothing here asks for.
    sub = Subsetter(opts)
    sub.populate(unicodes=unicodes)
    sub.subset(font)
    return font


def rename(font, style):
    family = "Luvie Sans"
    ps = f"LuvieSans-{style}"
    note = ("Luvie's UI font: Noto Sans cut down to Latin. Modified from the original "
            "by Luvie's tools/make-ui-font.py.")
    names = font["name"]
    # Drop typographic family/subfamily records; with only two styles the basic
    # family/subfamily pair (IDs 1 and 2) says everything.
    names.names = [n for n in names.names if n.nameID not in (16, 17, 21, 22)]
    for rec in list(names.names):
        if rec.nameID == 1:
            names.setName(family, 1, rec.platformID, rec.platEncID, rec.langID)
        elif rec.nameID == 2:
            names.setName(style, 2, rec.platformID, rec.platEncID, rec.langID)
        elif rec.nameID == 3:
            names.setName(f"{ps};Luvie", 3, rec.platformID, rec.platEncID, rec.langID)
        elif rec.nameID == 4:
            names.setName(f"{family} {style}", 4, rec.platformID, rec.platEncID, rec.langID)
        elif rec.nameID == 6:
            names.setName(ps, 6, rec.platformID, rec.platEncID, rec.langID)
        elif rec.nameID == 10:
            names.setName(note, 10, rec.platformID, rec.platEncID, rec.langID)
    if not names.getName(10, 3, 1, 0x409):
        names.setName(note, 10, 3, 1, 0x409)


def main():
    paths = {name: fetch(name) for name in SOURCES}
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for style in STYLES:
        font = subset(paths[f"NotoSans-{style}.ttf"], TEXT)
        rename(font, style)
        out = OUT_DIR / f"LuvieSans-{style}.ttf"
        font.save(out)
        print(f"wrote {out.relative_to(ROOT)}: {out.stat().st_size} bytes, "
              f"{len(font.getGlyphOrder())} glyphs")


if __name__ == "__main__":
    main()
