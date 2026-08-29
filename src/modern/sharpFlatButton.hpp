// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SHARP_FLAT_BUTTON_HPP
#define SHARP_FLAT_BUTTON_HPP

#include "accidentalText.hpp"
#include "toggleButton.hpp"
#include <FL/fl_draw.H>

// The sharp/flat switch in the harmony controls: a ToggleButton whose two faces
// are the accidental signs themselves (see accidentalText for the artwork)
// rather than the "#" and "b" stand-ins the text fonts we can count on limit us
// to. It carries no text label, so the tooltip is what names it.
class SharpFlatButton : public ToggleButton {
	// The sign's height in pixels, leaving air inside a 24px control.
	static constexpr int kGlyphH = 13;

protected:
	void drawContent(int X, int Y, int W, int H, Fl_Color bg) override {
		Fl_Color col = active() ? labelcolor() : fl_color_average(labelcolor(), bg, 0.4f);
		svgGlyph::draw(isOn() ? kSharpSvg : kFlatSvg, X + W / 2, Y + H / 2, col, kGlyphH);
	}

public:
	SharpFlatButton(int x, int y, int w, int h, bool sharp = true)
		: ToggleButton(x, y, w, h, nullptr, nullptr, sharp) {}
};

#endif
