#ifndef DENOM_BEAT_CHOICE_HPP
#define DENOM_BEAT_CHOICE_HPP

#include "modernChoice.hpp"
#include "timeSettings.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

// Dropdown that merges the time signature's denominator and its beat definition
// into one control (see timeSettings::denomBeatOptions):
//
//   2   4   8   8<dotted quaver>   8<dotted crotchet>
//
// The plain entries are counted in crotchets; the two dotted entries carry their
// own beat, drawn as a note glyph beside the digit. As with any glyph we cannot
// count on a font that has the musical symbols, so every entry is drawn by hand.
// A custom FLTK label type does the drawing, which gets us the same rendering in
// the popped-open menu and in the closed control: each menu item's label is just
// its index into timeSettings::denomBeatOptions, and the label type turns that
// index into a digit and (for the dotted variants) a note.
//
// Menu items take their colour from textcolor(); the closed control takes its
// from labelcolor().

// The dotted note glyphs, drawn from the SVG artwork in src/icons/. Both share
// the same viewBox, so they scale to a common height and sit on one baseline.
// The paths carry no fill, so nanosvg renders them black; we inject the wanted
// colour before rasterising (see beatUnitGlyphImage). These are minimised copies
// of src/icons/dottedEight.svg and dottedQuarter.svg (Inkscape cruft and the
// split-across-lines tag whitespace stripped, so the fill injection matches).
// viewBox is 56.94 wide by 96.56 tall.
inline const char* kDottedQuaverSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="-5 -10 56.936546 96.55584"><path d="M 41.38654,8.0120847 C 34.88354,1.4950847 25.750176,-4.145 24.032176,-10 l -0.31187,69.682251 c -4.907,-6.32 -11.658766,-5.602166 -18.3177661,-2.452166 -7.548,3.572 -13.24,11.873 -8.896,20.976 4.30500003,9.102 14.2930001,9.963 21.8770001,6.393 5.893,-2.801 10.658,-8.477 10.395,-15.178 v -56.364 c 9.418,1.419 17.179604,12.791665 20.777,23.074 1.619,-12.882 -1.403,-21.236 -8.169,-28.1190003 z"/><circle cx="45.816544" cy="72.21666" r="6.1200037"/></svg>)SVG";

inline const char* kDottedCrotchetSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="-5 -10 56.936546 96.55584"><path d="m 24.032176,-10 -0.31187,69.682251 c -4.907,-6.32 -11.658766,-5.602166 -18.3177661,-2.452166 -7.548,3.572 -13.24,11.873 -8.896,20.976 4.30500003,9.102 14.2930001,9.963 21.8770001,6.393 5.893,-2.801 10.658,-8.477 10.395,-15.178 l 0.114667,-79.0240302 z"/><circle cx="45.816544" cy="72.21666" r="6.1200037"/></svg>)SVG";

// The glyph's rasterised height in pixels; width follows from the viewBox aspect.
constexpr int kGlyphH = 14;

// A dotted-note glyph rasterised at kGlyphH and tinted to col. nanosvg parses the
// SVG once per (unit, colour); the results are cached because menu drawing repaints
// often (hover, open/close) and re-parsing on every paint is wasteful.
inline Fl_SVG_Image* beatUnitGlyphImage(timeSettings::BeatUnit u, Fl_Color col)
{
	using timeSettings::BeatUnit;

	static std::map<std::pair<int, unsigned>, Fl_SVG_Image*> cache;

	unsigned rgb = (unsigned)col;
	auto key = std::make_pair((int)u, rgb);
	auto it = cache.find(key);
	if (it != cache.end()) return it->second;

	const char* src = (u == BeatUnit::DottedQuaver) ? kDottedQuaverSvg : kDottedCrotchetSvg;

	uchar r, g, b;
	Fl::get_color(col, r, g, b);
	char hex[8];
	std::snprintf(hex, sizeof hex, "#%02x%02x%02x", r, g, b);

	// The artwork leaves fill unset, so give every path and circle the wanted fill.
	// Each needle ends in a space, so appending keeps the tag name intact.
	std::string svg = src;
	for (const char* tag : {"<path ", "<circle "}) {
		std::string needle = tag;
		std::string repl = needle + "fill=\"" + hex + "\" ";
		for (size_t p = 0; (p = svg.find(needle, p)) != std::string::npos; p += repl.size())
			svg.replace(p, needle.size(), repl);
	}

	auto* img = new Fl_SVG_Image(nullptr, svg.c_str());
	img->resize(kGlyphH, kGlyphH);   // proportional: aspect < 1 sets the width
	cache[key] = img;
	return img;
}

// Draw a dotted-note glyph centred on (cx, cy).
inline void drawBeatUnitGlyph(timeSettings::BeatUnit u, int cx, int cy, Fl_Color col)
{
	Fl_SVG_Image* img = beatUnitGlyphImage(u, col);
	img->draw(cx - img->w() / 2, cy - img->h() / 2);
}

namespace denomBeatLabel {

constexpr Fl_Labeltype kType   = FL_FREE_LABELTYPE;
constexpr int          kGlyphW = 8;    // room a note glyph occupies (hugs the SVG at kGlyphH)
constexpr int          kGlyphGap = 2;  // breathing space between the glyph and the ")"
constexpr int          kH      = 20;

// The dotted variants read "<digit>(<note>)": the note is an SVG glyph, the
// surrounding brackets are text. A trailing space in the opening bracket sets the
// digit off from it.
inline const char* kOpen  = " (";
inline const char* kClose = ")";

// The label string is the option's index into timeSettings::denomBeatOptions.
inline int indexOf(const Fl_Label* l)
{
	return (l && l->value && l->value[0]) ? l->value[0] - '0' : 0;
}

// Draw the denominator digit, and for the dotted variants a bracketed note glyph
// beside it.
inline void drawOption(int index, int X, int Y, int H, Fl_Color col,
                       Fl_Font font, Fl_Fontsize size)
{
	timeSettings::DenomBeat db = timeSettings::denomBeatAt(index);

	char digit[8];
	std::snprintf(digit, sizeof digit, "%d", db.denominator);

	fl_font(font, size);
	fl_color(col);
	int x = X;
	int digitW = (int)fl_width(digit);
	fl_draw(digit, x, Y, digitW, H, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
	x += digitW;

	if (db.beat == timeSettings::BeatUnit::Crotchet) return;

	int openW = (int)fl_width(kOpen);
	fl_draw(kOpen, x, Y, openW, H, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
	x += openW;

	drawBeatUnitGlyph(db.beat, x + kGlyphW / 2, Y + H / 2, col);
	x += kGlyphW + kGlyphGap;

	// drawBeatUnitGlyph left its own colour/line style set; restore for the text.
	fl_font(font, size);
	fl_color(col);
	fl_draw(kClose, x, Y, (int)fl_width(kClose), H, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
}

inline void draw(const Fl_Label* l, int X, int Y, int, int H, Fl_Align)
{
	drawOption(indexOf(l), X, Y, H, (Fl_Color)l->color, l->font, l->size);
}

// The drawn width of one option: the digit, plus the bracketed glyph for the
// dotted variants. Assumes the caller has set the font, or sets it from font/size.
inline int contentWidth(int index, Fl_Font font, Fl_Fontsize size)
{
	timeSettings::DenomBeat db = timeSettings::denomBeatAt(index);
	char digit[8];
	std::snprintf(digit, sizeof digit, "%d", db.denominator);
	fl_font(font, size);
	int w = (int)fl_width(digit);
	if (db.beat != timeSettings::BeatUnit::Crotchet)
		w += (int)fl_width(kOpen) + kGlyphW + kGlyphGap + (int)fl_width(kClose);
	return w;
}

inline void measure(const Fl_Label* l, int& W, int& H)
{
	W = contentWidth(indexOf(l), l->font, l->size);
	H = kH;
}

// FLTK label types are a global table, so register once. FLTK is single-threaded.
inline void ensureRegistered()
{
	static bool done = false;
	if (done) return;
	Fl::set_labeltype(kType, draw, measure);
	done = true;
}

}

class DenomBeatChoice : public ModernChoice {
	// One entry per timeSettings::denomBeatOptions, labelled with its index. copy()
	// gives each widget its own array, so the selection flag FLTK stamps on the
	// chosen item cannot leak between two DenomBeatChoices.
	static const Fl_Menu_Item* items() {
		static const Fl_Menu_Item menu[] = {
			{"0", 0, 0, 0, 0, (uchar)denomBeatLabel::kType, 0, 0, 0},
			{"1", 0, 0, 0, 0, (uchar)denomBeatLabel::kType, 0, 0, 0},
			{"2", 0, 0, 0, 0, (uchar)denomBeatLabel::kType, 0, 0, 0},
			{"3", 0, 0, 0, 0, (uchar)denomBeatLabel::kType, 0, 0, 0},
			{"4", 0, 0, 0, 0, (uchar)denomBeatLabel::kType, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0},
		};
		return menu;
	}

protected:
	void drawValue(int X, int Y, int W, int H) override {
		if (value() < 0) return;
		denomBeatLabel::drawOption(value(), X, Y, H, labelcolor(), labelfont(), labelsize());
	}

public:
	DenomBeatChoice(int x, int y, int w, int h) : ModernChoice(x, y, w, h) {
		denomBeatLabel::ensureRegistered();
		copy(items());
		value(timeSettings::denomBeatDefaultIndex);
	}

	// The width the closed control needs to show its widest option without slack:
	// content plus the left inset and the room the chevron takes on the right (see
	// ModernChoice::draw, which lays the value out in x+kInset .. w-kValuePad).
	int naturalWidth() const {
		int maxW = 0;
		for (int i = 0; i < (int)timeSettings::denomBeatOptions.size(); ++i)
			maxW = std::max(maxW, denomBeatLabel::contentWidth(i, labelfont(), labelsize()));
		return maxW + ModernChoice::kInset + ModernChoice::kValuePad;
	}

	int denominator() const {
		return timeSettings::denomBeatAt(value()).denominator;
	}

	timeSettings::BeatUnit beatUnit() const {
		return timeSettings::denomBeatAt(value()).beat;
	}

	void set(int denominator, timeSettings::BeatUnit beat) {
		value(timeSettings::denomBeatIndex(denominator, beat));
	}
};

#endif
