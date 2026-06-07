#ifndef SETTINGS_BUTTON_HPP
#define SETTINGS_BUTTON_HPP

#include <FL/Fl.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include <algorithm>
#include <cmath>
#include <cstring>

// Square gear button that drops the Settings menu. Sits at the right end of the
// tab bar, in line with the editor tabs. Subclasses Fl_Menu_Button so a click
// pops up the attached menu and item shortcuts (e.g. Cmd+S) keep working.
class SettingsButton : public Fl_Menu_Button {
    bool hovered = false;

    void drawGear() {
        const double cx = x() + w() / 2.0;
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
        double hole = R * 0.42;
        fl_color(bg());
        fl_pie(int(cx - hole), int(cy - hole), int(2 * hole), int(2 * hole), 0, 360);
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

    // Pixel width FLTK will give the popped-up menu window. Mirrors the layout
    // math in Fl_Menu.cxx (item label + toggle/checkbox column, plus the
    // shortcut and modifier columns and the window border) so we can right-align
    // the menu precisely against the window edge.
    int menuPixelWidth() const {
        const Fl_Menu_Item* m = menu();
        if (!m) return 0;
        fl_font(textfont(), textsize());
        int W = 0, shortcutsW = 0, modifiersW = 0, hh = 0;
        for (const Fl_Menu_Item* it = m; it && it->text; it = it->next()) {
            int w1 = it->measure(&hh, this);
            if (w1 > W) W = w1;
            if (it->shortcut_) {
                const char* k;
                const char* s = fl_shortcut_label(it->shortcut_, &k);
                if (fl_utf_nb_char((const unsigned char*)k, (int)strlen(k)) <= 4) {
                    int a = (int)fl_width(s, (int)(k - s));
                    if (a > modifiersW) modifiersW = a;
                    int b = (int)fl_width(k) + 4;
                    if (b > shortcutsW) shortcutsW = b;
                } else {
                    int a = (int)fl_width(s) + 4;
                    if (a > modifiersW + shortcutsW) modifiersW = a - shortcutsW;
                }
            }
        }
        return W + shortcutsW + modifiersW + 2 * Fl::box_dx(FL_UP_BOX) + 7;
    }

    // Drop the menu from directly below the gear, right-aligned so it stays
    // inside the app window instead of popping up at the cursor (which would
    // spill past the window's right edge at this top-right position).
    void openMenu() {
        const Fl_Menu_Item* m = menu();
        if (!m) return;
        int mx = std::max(0, x() + w() - menuPixelWidth());
        const Fl_Menu_Item* sel = m->popup(mx, y() + h(), nullptr, nullptr, this);
        picked(sel);
    }

    int handle(int event) override {
        if (event == FL_ENTER) { hovered = true;  redraw(); return 1; }
        if (event == FL_LEAVE) { hovered = false; redraw(); return 1; }
        // Fl_Menu_Button only auto-pops on right-click when it has no box, so
        // drop the menu on a left-click ourselves. Ignore other buttons.
        if (event == FL_PUSH) {
            if (Fl::event_button() == FL_LEFT_MOUSE) openMenu();
            return 1;
        }
        return Fl_Menu_Button::handle(event);
    }

public:
    SettingsButton(int x, int y, int w, int h)
        : Fl_Menu_Button(x, y, w, h, nullptr) {
        box(FL_NO_BOX);
        color(FL_WHITE);
    }
};

#endif
