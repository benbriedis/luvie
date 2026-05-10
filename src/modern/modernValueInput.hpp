#ifndef MODERN_VALUE_INPUT_HPP
#define MODERN_VALUE_INPUT_HPP

#include <FL/Fl.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_draw.H>
#include "../panelStyle.hpp"

class ModernValueInput : public Fl_Value_Input {
    Fl_Color borderCol = panelCtrlBorder;

    void draw() override {
        if (Fl::focus() == &input) {
            Fl_Value_Input::draw();
            fl_color(borderCol);
            fl_rect(x(), y(), w(), h());
            return;
        }
        fl_color(color());
        fl_rectf(x(), y(), w(), h());
        char buf[64];
        format(buf);
        fl_font(textfont(), textsize());
        fl_color(textcolor());
        fl_draw(buf, x() + 1, y() + 1, w() - 2, h() - 2, FL_ALIGN_CENTER);
        fl_color(borderCol);
        fl_rect(x(), y(), w(), h());
    }

public:
    ModernValueInput(int x, int y, int w, int h)
        : Fl_Value_Input(x, y, w, h) {
        box(FL_FLAT_BOX);
    }

    void setBorderColor(Fl_Color c) { borderCol = c; }
};

#endif
