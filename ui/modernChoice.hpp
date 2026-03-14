#ifndef MODERN_CHOICE_HPP
#define MODERN_CHOICE_HPP

#include <FL/Fl_Choice.H>
#include <FL/fl_draw.H>

class ModernChoice : public Fl_Choice {
    bool hovered = false;

    static constexpr Fl_Color bgNormal  = 0x1E293B00;
    static constexpr Fl_Color bgHover   = 0x2D374800;
    static constexpr Fl_Color borderCol = 0x47556900;
    static constexpr Fl_Color arrowCol  = 0x94A3B800;

    void draw() override {
        fl_color(hovered ? bgHover : bgNormal);
        fl_rectf(x(), y(), w(), h());

        fl_color(borderCol);
        fl_rect(x(), y(), w(), h());

        const char* lbl = value() >= 0 ? text(value()) : nullptr;
        if (lbl) {
            fl_font(labelfont(), labelsize());
            fl_color(FL_WHITE);
            fl_draw(lbl, x() + 8, y(), w() - 24, h(), FL_ALIGN_LEFT | FL_ALIGN_CENTER);
        }

        // chevron
        int cx = x() + w() - 13;
        int cy = y() + h() / 2 + 1;
        fl_color(arrowCol);
        fl_line_style(FL_SOLID, 2);
        fl_line(cx - 4, cy - 2, cx,     cy + 2);
        fl_line(cx,     cy + 2, cx + 4, cy - 2);
        fl_line_style(0);
    }

    int handle(int event) override {
        if (event == FL_ENTER) { hovered = true;  redraw(); return 1; }
        if (event == FL_LEAVE) { hovered = false; redraw(); return 1; }
        return Fl_Choice::handle(event);
    }

public:
    ModernChoice(int x, int y, int w, int h)
        : Fl_Choice(x, y, w, h, nullptr) { box(FL_NO_BOX); }
};

#endif
