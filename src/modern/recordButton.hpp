// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef RECORD_BUTTON_HPP
#define RECORD_BUTTON_HPP

#include "modernButton.hpp"
#include "svgGlyph.hpp"
#include <FL/fl_draw.H>

// The record-arm toggle: a ModernButton whose face is the familiar red dot
// rather than the word "Record". It carries no text label, so the tooltip is
// what names it.
//
// The artwork is src/icons/record.svg, minimised the way svgGlyph wants it.
// svgGlyph tints a whole piece of artwork one colour, so the bordered dot is
// drawn as the one circle twice: a larger disc in the label colour for the
// border, then the red dot on top of it.
inline const char* kRecordSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 16 16"><circle cx="8" cy="8" r="8"/></svg>)SVG";

class RecordButton : public ModernButton {
	// Border disc and dot heights in pixels, leaving air inside a 24px control.
	// Both even, so the two share a centre pixel for pixel.
	static constexpr int kRingH = 14;
	static constexpr int kDotH  = 10;
	static constexpr Fl_Color kDotColor = 0xEF444400;

protected:
	void drawContent(int X, int Y, int W, int H, Fl_Color bg) override {
		Fl_Color ring = labelcolor();
		Fl_Color dot  = kDotColor;
		if (!active()) {
			ring = fl_color_average(ring, bg, 0.4f);
			dot  = fl_color_average(dot,  bg, 0.4f);
		}
		int cx = X + W / 2, cy = Y + H / 2;
		svgGlyph::draw(kRecordSvg, cx, cy, ring, kRingH);
		svgGlyph::draw(kRecordSvg, cx, cy, dot,  kDotH);
	}

public:
	RecordButton(int x, int y, int w, int h) : ModernButton(x, y, w, h) {}
};

#endif
