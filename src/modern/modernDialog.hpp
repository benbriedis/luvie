// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MODERN_DIALOG_HPP
#define MODERN_DIALOG_HPP

#include <string>
#include <vector>

// Modal message boxes in the app's own style, in place of FLTK's stock
// fl_choice() / fl_alert(): a bold title over a short explanation, on the
// overlays' colours, with flat buttons along the bottom. Each is a top-level
// window of its own, so it works before the main window exists, and it blocks
// until answered. GUI thread only.
namespace modernDialog {

struct Button {
	std::string label;
	enum Role {
		Normal,       // right-aligned, plain
		Primary,      // right-aligned, accent-coloured; Enter presses it
		Separate,     // plain, but set apart at the left -- the "Don't Save" slot
	} role = Normal;
};

// Show `title` and `message` with `buttons` and wait for one to be pressed.
// Returns the index of the pressed button, or `cancelIndex` if the dialog is
// dismissed with Escape or the window's close button. Right-aligned buttons
// keep their order, left to right.
int choice(const std::string& title, const std::string& message,
           const std::vector<Button>& buttons, int cancelIndex);

// A message with a single OK button.
void alert(const std::string& title, const std::string& message);

}

#endif
