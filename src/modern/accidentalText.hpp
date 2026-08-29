// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef ACCIDENTAL_TEXT_HPP
#define ACCIDENTAL_TEXT_HPP

#include "svgGlyph.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>

// Note and chord names are written with "#" and "b" in the source -- they have to
// be, they are C strings -- but drawn with the real accidental signs, since we
// cannot count on a font that carries them. The artwork is src/icons/sharp.svg
// and flat.svg, minimised here the way svgGlyph wants it. Each viewBox hugs its
// ink, so the two signs scale to a common height and take up only the width they
// need, which is what lets them sit inside a run of text.
inline const char* kSharpSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="83.16 17.87 345.69 494.13"><path d="M418.562,173.34c5.999-1.291,10.281-6.582,10.281-12.724V103.86c0-3.927-1.775-7.649-4.834-10.124c-3.058-2.466-7.07-3.425-10.912-2.6l-51.621,11.093V30.884c0-3.856-1.713-7.515-4.672-9.99c-2.964-2.475-6.869-3.507-10.662-2.816l-38.686,7.013c-6.192,1.121-10.694,6.51-10.694,12.805v78.242l-80.658,17.333V64.117c0-3.856-1.713-7.514-4.672-9.99c-2.958-2.475-6.864-3.506-10.662-2.816l-38.69,7.004c-6.192,1.12-10.693,6.511-10.693,12.806v76.25l-57.948,12.456c-5.999,1.282-10.281,6.59-10.281,12.724v56.756c0,3.927,1.776,7.649,4.834,10.124c3.062,2.466,7.07,3.426,10.917,2.601l52.478-11.281v108.39l-57.948,12.456c-5.999,1.282-10.281,6.582-10.281,12.715v56.737c0,3.928,1.776,7.649,4.834,10.125c3.062,2.466,7.07,3.425,10.917,2.6l52.478-11.281v76.492c0,3.856,1.712,7.515,4.672,9.99c2.959,2.476,6.864,3.507,10.662,2.816l38.686-6.995c6.192-1.12,10.698-6.51,10.698-12.805v-83.397l80.658-17.334v74.502c0,3.865,1.712,7.524,4.672,9.99c2.96,2.475,6.865,3.506,10.662,2.815l38.686-7.004c6.192-1.121,10.694-6.51,10.694-12.805V377.35l57.087-12.267c5.999-1.291,10.281-6.582,10.281-12.724v-56.729c0-3.927-1.775-7.649-4.834-10.124c-3.058-2.466-7.07-3.426-10.912-2.6l-51.621,11.093v-108.39L418.562,173.34z M296.761,307.906l-80.658,17.326V216.85l80.658-17.334V307.906z"/></svg>)SVG";

inline const char* kFlatSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="129.26 0 253.48 512"><path d="M200.438,214.712V0h-71.18v512c0,0,170.389-50.606,236.182-162.99C424.052,248.893,324.927,139.024,200.438,214.712z M300.508,302.609c-6.37,82.823-100.117,126.984-100.117,126.984v-156.27C239.449,239.14,305.394,239.14,300.508,302.609z"/></svg>)SVG";

// Text in which "#" and "b" are drawn as accidental signs.
namespace accidentalText {

// A sign is drawn a little taller than the capitals it sits beside -- about
// seven eighths of the point size, against a cap height of some three quarters
// -- which is the proportion an engraved accidental keeps against its note.
inline int glyphHeight(Fl_Fontsize size) { return (size * 7 + 4) / 8; }

// How far below the baseline a sign reaches. The flat's bulb rests on it, like a
// letter; the sharp is symmetric about its middle, so it drops a little and
// straddles the line instead.
inline int signDrop(const char* svg, Fl_Fontsize size)
{
	return (svg == kFlatSvg) ? 0 : size / 8;
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
// the signs standing on the text's own baseline.
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
			int bottom = baseline + signDrop(svg, size);
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
