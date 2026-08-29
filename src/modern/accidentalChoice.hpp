// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef ACCIDENTAL_CHOICE_HPP
#define ACCIDENTAL_CHOICE_HPP

#include "accidentalText.hpp"
#include "modernChoice.hpp"

// A ModernChoice for note and chord names: "C#" and "Bb" and "7(b9)" are drawn
// with the real accidental signs, both in the closed control and in the menu.
// Populate it as usual, then call styleItems().
class AccidentalChoice : public ModernChoice {
protected:
	void drawValue(int X, int Y, int W, int H) override {
		const char* lbl = value() >= 0 ? text(value()) : nullptr;
		if (!lbl) return;
		accidentalText::draw(lbl, X, Y, W, H, FL_ALIGN_LEFT,
		                     labelcolor(), labelfont(), labelsize());
	}

public:
	AccidentalChoice(int x, int y, int w, int h) : ModernChoice(x, y, w, h) {
		accidentalText::ensureRegistered();
	}

	// The closed control is ours to draw, but the popped-open menu is FLTK's, and
	// it goes by the label type stamped on each item -- so stamp them. Only the
	// items that hold an accidental: the rest keep FLTK's own label drawing, which
	// is what reads the "&&" in a submenu name like "Jazz && Blues". Call after
	// the adds, which may have moved the items about.
	void styleItems() {
		auto* items = const_cast<Fl_Menu_Item*>(menu());
		if (!items) return;
		for (int i = 0; i < size(); ++i)
			if (accidentalText::hasAccidental(items[i].label()))
				items[i].labeltype(accidentalText::kType);
	}
};

#endif
