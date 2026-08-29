// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef ACCIDENTAL_TEXT_HPP
#define ACCIDENTAL_TEXT_HPP

#include "svgGlyph.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>

// Note and chord names are written with "#" and "b" in the source -- they have to
// be, they are C strings -- but drawn with the real accidental signs. The artwork
// is src/icons/sharp.svg and flat.svg, minimised here the way svgGlyph wants it
// (sharp.svg's second path, a sliver a fraction of a unit across, is dropped
// along with the editor cruft). Each viewBox hugs its ink, so the two signs
// scale to a common height and take up only the width they need, which is what
// lets them sit inside a run of text. Re-minimise after editing the artwork:
// these constants are the copy that gets compiled in, not the files.
//
// The signs could instead be had as text: U+266F and U+266D are ordinary
// characters in Miscellaneous Symbols, which DejaVu carries, so unlike the
// Musical Symbols block the dotted notes would need (see denomBeatChoice), a
// font can be expected to draw them. The two were compared side by side and the
// artwork won -- and it draws the same everywhere, where the characters lean on
// whatever the installed fonts happen to fall back to.
inline const char* kSharpSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="83.16 18.75 345.69 490.94"><path d="m 418.562,162.53752 c 5.999,-1.291 10.281,-6.582 10.281,-12.724 v -43.25787 c 0,-3.927 -1.775,-7.648998 -4.834,-10.123998 -3.058,-2.466 -7.07,-3.425 -10.912,-2.6 L 356.10954,105.80912 V 31.768472 c 0,-3.856 -1.713,-7.515 -4.672,-9.99 -2.964,-2.475 -6.869,-3.507 -10.662,-2.816 l -26.32327,4.409273 c -6.192,1.121 -10.694,6.51 -10.694,12.805 V 117.1144 L 210.2804,138.25609 V 66.206441 c 0,-3.856 -1.713,-7.514 -4.672,-9.99 -2.958,-2.475 -6.864,-3.506 -10.662,-2.816 l -27.78317,2.601516 c -6.192,1.12 -10.693,6.511 -10.693,12.806 V 147.75361 L 93.439,162.52265 c -5.999,1.282 -10.281,6.59 -10.281,12.724 v 43.25787 c 0,3.927 1.776,7.649 4.834,10.124 3.062,2.466 7.07,3.426 10.917,2.601 l 57.56123,-13.59405 V 344.94224 L 93.439,352.47402 c -5.999,1.282 -10.281,6.582 -10.281,12.715 v 48.62271 c 0,3.928 1.776,7.649 4.834,10.125 3.062,2.466 7.07,3.425 10.917,2.6 l 57.56123,-13.59404 0,83.72927 c 0,3.856 1.712,7.515 4.672,9.99 2.959,2.476 6.864,3.507 10.662,2.816 l 27.77917,-2.59252 c 6.192,-1.12 10.698,-6.51 10.698,-12.805 l 0,-90.63427 93.47787,-21.14269 0,81.73927 c 0,3.865 1.712,7.524 4.672,9.99 2.96,2.475 6.865,3.506 10.662,2.815 l 26.32327,-4.40028 c 6.192,-1.121 10.694,-6.51 10.694,-12.805 l 0,-88.64527 62.45346,-13.15147 c 5.999,-1.291 10.281,-6.582 10.281,-12.724 l 0,-41.37744 c 0,-3.927 -1.775,-7.649 -4.834,-10.124 -3.058,-2.466 -7.07,-3.426 -10.912,-2.6 l -56.98746,11.97747 V 175.69099 Z M 303.75727,314.30103 210.2794,335.43573 V 208.13696 l 93.47787,-21.1427 z"/></svg>)SVG";

inline const char* kFlatSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="133.82 0 248.45 512"><path d="M 195.896,214.712 V 0 l -62.08016,0 v 512 c 0,0 165.83116,-50.606 231.62416,-162.99 C 424.052,248.893 320.385,139.024 195.896,214.712 Z m 115.95907,83.28527 C 305.48507,380.82027 195.849,429.593 195.849,429.593 v -156.27 c 39.058,-34.183 120.89207,-38.79473 116.00607,24.67427 z"/></svg>)SVG";

// Text in which "#" and "b" are drawn as accidental signs.
namespace accidentalText {

// A sign is drawn taller than the capitals it sits beside -- all but the point
// size itself, against a cap height of some three quarters of it -- which is
// about the proportion an engraved accidental keeps against its note.
inline int glyphHeight(Fl_Fontsize size) { return (size * 19 + 10) / 20; }

// How far a sign is lifted off the text's baseline. The flat's bulb would rest
// on it like a letter, but reads better standing a shade clear; the sharp is
// symmetric about its middle, so for the two to look level it has to hang the
// lower of them, and sits on the baseline itself.
inline int signLift(const char* svg, Fl_Fontsize size)
{
	return (svg == kFlatSvg) ? (size + 5) / 10 : 0;
}

// Air either side of a sign: its viewBox hugs the ink, so it brings no side
// bearing of its own and would otherwise touch the letters.
constexpr int kBearing = 1;

// Which sign, if any, the character at s[i] spells, as its artwork. A '#' is
// always an accidental. A 'b' is one only where it spells a flat -- after a note
// letter ("Bb", "Db4") or before a degree ("7(b9)") -- so that the 'b' in
// "Bebop" or "Major blues" stays a letter.
inline const char* signAt(const char* s, int i)
{
	if (s[i] == '#') return kSharpSvg;
	if (s[i] != 'b') return nullptr;
	if (i > 0 && s[i - 1] >= 'A' && s[i - 1] <= 'G') return kFlatSvg;
	return (s[i + 1] >= '0' && s[i + 1] <= '9') ? kFlatSvg : nullptr;
}

// Hand the caller each stretch of plain text and each accidental of s, in the
// order they appear: text(p, n) for a run of n characters at p, glyph(svg) for
// a sign. Measuring and drawing walk the string the same way, so they agree.
template <class Text, class Glyph>
inline void split(const char* s, Text text, Glyph glyph)
{
	const char* run = s;
	for (const char* p = s; ; ++p) {
		const char* sign = *p ? signAt(s, (int)(p - s)) : nullptr;
		if (*p && !sign) continue;
		if (p > run) text(run, (int)(p - run));
		if (!*p) return;
		glyph(sign);
		run = p + 1;
	}
}

inline bool hasAccidental(const char* s)
{
	if (!s) return false;
	for (int i = 0; s[i]; ++i)
		if (signAt(s, i)) return true;
	return false;
}

// The drawn width of s, signs included.
inline int width(const char* s, Fl_Font font, Fl_Fontsize size)
{
	if (!s) return 0;
	fl_font(font, size);
	int gh = glyphHeight(size);
	int w  = 0;
	split(s, [&](const char* p, int n) { w += (int)fl_width(p, n); },
	         [&](const char* svg) { w += svgGlyph::width(svg, gh) + 2 * kBearing; });
	return w;
}

// Draw s in the X,Y,W,H box, aligned left, right or (by default) centred, with
// the signs riding just clear of the text's own baseline.
inline void draw(const char* s, int X, int Y, int W, int H, Fl_Align align,
                 Fl_Color col, Fl_Font font, Fl_Fontsize size)
{
	if (!s) return;
	fl_font(font, size);

	int x = X;
	if      (align & FL_ALIGN_RIGHT) x = X + W - width(s, font, size);
	else if (!(align & FL_ALIGN_LEFT)) x = X + (W - width(s, font, size)) / 2;

	int baseline = Y + H / 2 + fl_height() / 2 - fl_descent();
	int gh       = glyphHeight(size);

	split(s,
		[&](const char* p, int n) {
			// A glyph leaves its own colour set, so restore before every run.
			fl_font(font, size);
			fl_color(col);
			fl_draw(p, n, x, baseline);
			x += (int)fl_width(p, n);
		},
		[&](const char* svg) {
			int gw     = svgGlyph::width(svg, gh);
			int bottom = baseline - signLift(svg, size);
			svgGlyph::draw(svg, x + kBearing + gw / 2, bottom - gh / 2, col, gh);
			x += gw + 2 * kBearing;
		});
}

// The label type that gets accidental signs into a menu: FLTK draws the popped-
// open menu itself and goes by the label type stamped on each item (see
// AccidentalChoice::styleItems). FL_FREE_LABELTYPE itself is taken -- the
// table is global, and denomBeatLabel has it.
constexpr Fl_Labeltype kType = (Fl_Labeltype)(FL_FREE_LABELTYPE + 1);

inline void drawLabel(const Fl_Label* l, int X, int Y, int W, int H, Fl_Align a)
{
	draw(l->value, X, Y, W, H, a, (Fl_Color)l->color, l->font, l->size);
}

inline void measureLabel(const Fl_Label* l, int& W, int& H)
{
	fl_font(l->font, l->size);
	W = width(l->value, l->font, l->size);
	H = fl_height();
}

// FLTK label types are a global table, so register once. FLTK is single-threaded.
inline void ensureRegistered()
{
	static bool done = false;
	if (done) return;
	Fl::set_labeltype(kType, drawLabel, measureLabel);
	done = true;
}

}

#endif
