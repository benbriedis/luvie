#ifndef SETTINGS_BUTTON_HPP
#define SETTINGS_BUTTON_HPP

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <functional>

// Square gear button at the right end of the tab bar, in line with the editor
// tabs. A left-click fires onClick; the owner pops up a context-menu-style
// SettingsMenuPopup in response (see settingsMenuPopup.hpp).
class SettingsButton : public Fl_Box {
    bool hovered = false;

    void drawGear() {
        // Centre on the area right of the left divider (x()+1 .. x()+w()),
        // which is a pixel narrower than the full button, else the gear reads
        // as shifted left.
        const double cx = x() + 1 + (w() - 1) / 2.0;
        const double cy = y() + h() / 2.0;
        const double R  = std::min(w(), h()) * 0.30;   // tooth-tip radius
        const double r  = R * 0.72;                     // tooth-valley radius
        const int    teeth = 8;

        fl_color(active() ? 0x4B556300 : 0x9CA3AF00);
        fl_begin_complex_polygon();
        for (int i = 0; i < teeth * 2; ++i) {
            double a0  = (i)     * M_PI / teeth;
            double a1  = (i + 1) * M_PI / teeth;
            double rad = (i % 2 == 0) ? R : r;
            fl_vertex(cx + rad * std::cos(a0), cy + rad * std::sin(a0));
            fl_vertex(cx + rad * std::cos(a1), cy + rad * std::sin(a1));
        }
        fl_end_complex_polygon();

        // Punch the hub hole in the (possibly hovered) background colour.
        // Use a polygon with the same double-precision centre as the teeth so
        // the hole is exactly concentric (fl_pie's integer coords drift off).
        double hole = R * 0.42;
        fl_color(bg());
        fl_begin_complex_polygon();
        for (int i = 0; i < 32; ++i) {
            double a = i * 2.0 * M_PI / 32;
            fl_vertex(cx + hole * std::cos(a), cy + hole * std::sin(a));
        }
        fl_end_complex_polygon();
    }

    Fl_Color bg() const {
        return hovered ? fl_color_average(color(), FL_WHITE, 0.7f) : color();
    }

    void draw() override {
        fl_color(bg());
        fl_rectf(x(), y(), w(), h());

        // Left divider, matching the tab separators.
        fl_color(0xD1D5DB00);
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
        color(FL_WHITE);
    }
};

#endif
