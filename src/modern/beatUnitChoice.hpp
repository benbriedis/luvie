#ifndef BEAT_UNIT_CHOICE_HPP
#define BEAT_UNIT_CHOICE_HPP

#include "modernChoice.hpp"
#include "timeSettings.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>

// Dropdown for the beat definition: crotchet, dotted crotchet or minim.
//
// The options are pictures, not words, and we cannot count on a font that has
// the musical glyphs (U+1D15E and friends are outside the BMP and missing from
// most UI fonts), so the notes are drawn with primitives. A custom FLTK label
// type does the drawing, which gets us the same glyph in the popped-open menu
// and in the closed control: each menu item's label is just its index into
// timeSettings::beatUnits, and the label type turns that index into a note.
//
// Menu items take their colour from textcolor(); the closed control takes its
// from labelcolor().

// Draw one note about the point (cx, cy), which is the glyph's centre in both
// axes: filled head for the crotchets, hollow head for the minim, stem up on the
// right, augmentation dot for the dotted crotchet.
inline void drawBeatUnitGlyph(timeSettings::BeatUnit u, int cx, int cy, Fl_Color col)
{
	constexpr int headW = 9;
	constexpr int headH = 7;
	constexpr int stemH = 13;   // from the head's centre upwards

	// The note is bottom-heavy — head below, stem above — so its centre sits well
	// above the head. Offsetting the head by this puts the whole glyph's bounding
	// box, not just the head, in the middle of the row.
	constexpr int headCy = (stemH - headH / 2) / 2;

	// The dotted crotchet is wider than the others; nudge it left so all three
	// sit on the same optical centre rather than drifting as the value changes.
	if (u == timeSettings::BeatUnit::DottedCrotchet) cx -= 2;

	const int headX = cx - headW / 2;
	const int headY = cy + headCy - headH / 2;
	const int stemX = headX + headW - 1;
	const int stemY = headY + headH / 2;

	fl_color(col);
	if (u == timeSettings::BeatUnit::Minim) {
		fl_line_style(FL_SOLID, 2);
		fl_arc(headX, headY, headW, headH, 0, 360);
		fl_line_style(0);
	} else {
		fl_pie(headX, headY, headW, headH, 0, 360);
	}

	fl_line_style(FL_SOLID, 2);
	fl_line(stemX, stemY, stemX, stemY - stemH);
	fl_line_style(0);

	if (u == timeSettings::BeatUnit::DottedCrotchet)
		fl_pie(stemX + 3, stemY - 1, 4, 4, 0, 360);
}

namespace beatUnitLabel {

constexpr Fl_Labeltype kType = FL_FREE_LABELTYPE;
constexpr int          kW    = 18;
constexpr int          kH    = 20;

// The label string is the beat unit's index into timeSettings::beatUnits.
inline timeSettings::BeatUnit unitOf(const Fl_Label* l)
{
	int idx = (l && l->value && l->value[0]) ? l->value[0] - '0' : 0;
	return timeSettings::beatUnitAt(idx);
}

inline void draw(const Fl_Label* l, int X, int Y, int W, int H, Fl_Align)
{
	drawBeatUnitGlyph(unitOf(l), X + kW / 2, Y + H / 2, (Fl_Color)l->color);
}

inline void measure(const Fl_Label*, int& W, int& H)
{
	W = kW;
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

class BeatUnitChoice : public ModernChoice {
	// One entry per timeSettings::beatUnits, labelled with its index. copy() gives
	// each widget its own array, so the selection flag FLTK stamps on the chosen
	// item cannot leak between two BeatUnitChoices.
	static const Fl_Menu_Item* items() {
		static const Fl_Menu_Item menu[] = {
			{"0", 0, 0, 0, 0, (uchar)beatUnitLabel::kType, 0, 0, 0},
			{"1", 0, 0, 0, 0, (uchar)beatUnitLabel::kType, 0, 0, 0},
			{"2", 0, 0, 0, 0, (uchar)beatUnitLabel::kType, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0},
		};
		return menu;
	}

protected:
	void drawValue(int X, int Y, int W, int H) override {
		if (value() < 0) return;
		drawBeatUnitGlyph(beatUnit(), X + beatUnitLabel::kW / 2, Y + H / 2, labelcolor());
	}

public:
	BeatUnitChoice(int x, int y, int w, int h) : ModernChoice(x, y, w, h) {
		beatUnitLabel::ensureRegistered();
		copy(items());
		value(timeSettings::beatUnitDefaultIndex);
	}

	timeSettings::BeatUnit beatUnit() const {
		return timeSettings::beatUnitAt(value());
	}

	void setBeatUnit(timeSettings::BeatUnit u) {
		value(timeSettings::beatUnitIndex(u));
	}
};

#endif
