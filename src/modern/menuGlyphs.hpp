// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MENU_GLYPHS_HPP
#define MENU_GLYPHS_HPP

#include <FL/fl_draw.H>
#include <FL/Enumerations.H>
#include "svgGlyph.hpp"

// Menu decorations drawn as geometry rather than as characters. These were
// U+25B6 and U+2713 sitting in the item's label, which draws as an empty box on
// a typical Linux desktop: FLTK is built here against Xft without Pango, so a
// character the label font lacks is not looked up in any other font, and Nimbus
// Sans -- what FL_HELVETICA resolves to there -- carries neither. Same reasoning
// as svgGlyph.hpp; the arrow is drawn from artwork, the tick from plain lines.
namespace menuGlyphs {

// The gutter a menu reserves down its left edge for tick(), in pixels.
inline constexpr int tickSlotW = 17;

// The submenu arrow is src/icons/submenuArrow.svg, minus the XML declaration.
// Re-copy it after editing the file: this constant is what gets compiled in.
inline const char* kSubmenuArrowSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 7 10"><path fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round" d="M1.25 1 L1.25 9 L6.25 5 Z"/></svg>)SVG";

// Filled right-pointing triangle, centred on (cx, cy), `size` pixels tall.
inline void submenuArrow(int cx, int cy, Fl_Color col, int size = 10)
{
	svgGlyph::draw(kSubmenuArrowSvg, cx, cy, col, size);
}

// Tick, centred on (cx, cy).
inline void tick(int cx, int cy, Fl_Color col, int size = 10)
{
	const int left = cx - size / 2, right = cx + size / 2;
	fl_color(col);
	fl_line_style(FL_SOLID, 2, nullptr);
	fl_line(left, cy, left + size / 3, cy + size / 3);
	fl_line(left + size / 3, cy + size / 3, right, cy - size / 2);
	fl_line_style(0);
}

}

#endif
