#ifndef MODERN_BUTTON_HPP
#define MODERN_BUTTON_HPP

#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

class ModernButton : public Fl_Button {
    bool hovered = false;

    static constexpr Fl_Color bgNormal  = 0x1E293B00;
    static constexpr Fl_Color bgHover   = 0x2D374800;
    static constexpr Fl_Color borderCol = 0x47556900;

    void draw() override {
        fl_color(hovered ? bgHover : bgNormal);
        fl_rectf(x(), y(), w(), h());

        fl_color(borderCol);
        fl_rect(x(), y(), w(), h());

        if (label()) {
            fl_font(labelfont(), labelsize());
            fl_color(FL_WHITE);
            fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
        }
    }

    int handle(int event) override {
        if (event == FL_ENTER) { hovered = true;  redraw(); return 1; }
        if (event == FL_LEAVE) { hovered = false; redraw(); return 1; }
        return Fl_Button::handle(event);
    }

public:
    ModernButton(int x, int y, int w, int h, const char* label = nullptr)
        : Fl_Button(x, y, w, h, label) { box(FL_NO_BOX); }
};

#endif
