// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SETTINGS_BUTTON_HPP
#define SETTINGS_BUTTON_HPP

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include "theme.hpp"
#include "modern/svgGlyph.hpp"
#include <algorithm>
#include <functional>

// The artwork is src/icons/gear.svg, minus the XML declaration. Re-copy it after
// editing the file: this constant is what gets compiled in.
inline const char* kGearSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 24 24"><path fill="currentColor" fill-rule="evenodd" d="M10.27 3.98 L10.54 1.10 L13.46 1.10 L13.73 3.98 L16.45 5.11 L18.67 3.26 L20.74 5.33 L18.89 7.55 L20.02 10.27 L22.90 10.54 L22.90 13.46 L20.02 13.73 L18.89 16.45 L20.74 18.67 L18.67 20.74 L16.45 18.89 L13.73 20.02 L13.46 22.90 L10.54 22.90 L10.27 20.02 L7.55 18.89 L5.33 20.74 L3.26 18.67 L5.11 16.45 L3.98 13.73 L1.10 13.46 L1.10 10.54 L3.98 10.27 L5.11 7.55 L3.26 5.33 L5.33 3.26 L7.55 5.11Z M8 12 A4 4 0 1 0 16 12 A4 4 0 1 0 8 12Z"/></svg>)SVG";

// Square gear button at the right end of the tab bar, in line with the editor
// tabs. A left-click fires onClick; the owner pops up a context-menu-style
// SettingsMenuPopup in response (see settingsMenuPopup.hpp).
class SettingsButton : public Fl_Box {
    bool hovered = false;

    void drawGear() {
        // 22px across in the stock 35px button. Kept even, so the glyph centres
        // on a whole pixel: the area right of the left divider (x()+1 ..
        // x()+w()), a pixel narrower than the full button, else the gear reads
        // as shifted left. The hub is a hole in the artwork, so the (possibly
        // hovered) background shows through it.
        const int size = (int)(std::min(w(), h()) * 0.64) & ~1;
        const int cx   = x() + 1 + (w() - 1) / 2;
        const int cy   = y() + h() / 2;
        svgGlyph::draw(kGearSvg, cx, cy,
                       active() ? theme().gearIcon : theme().gearIconInactive, size);
    }

    Fl_Color bg() const {
        return hovered ? fl_color_average(theme().tabBarBg, FL_WHITE, 0.7f) : theme().tabBarBg;
    }

    void draw() override {
        fl_color(bg());
        fl_rectf(x(), y(), w(), h());

        // Left divider, matching the tab separators.
        fl_color(theme().tabSeparator);
        fl_yxline(x(), y() + 4, y() + h() - 2);

        drawGear();
    }

    int handle(int event) override {
        if (event == FL_ENTER) { hovered = true;  redraw(); return 1; }
        if (event == FL_LEAVE) { hovered = false; redraw(); return 1; }
        if (event == FL_PUSH) {
            if (Fl::event_button() == FL_LEFT_MOUSE && onClick) onClick();
            return 1;
        }
        return Fl_Box::handle(event);
    }

public:
    // Fired on left-click; the owner opens the settings menu in response.
    std::function<void()> onClick;

    SettingsButton(int x, int y, int w, int h)
        : Fl_Box(x, y, w, h) {
        box(FL_NO_BOX);
    }
};

#endif
