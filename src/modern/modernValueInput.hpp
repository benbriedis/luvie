#ifndef MODERN_VALUE_INPUT_HPP
#define MODERN_VALUE_INPUT_HPP

#include <FL/Fl.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_draw.H>
#include <cmath>
#include <cstdlib>
#include "panelStyle.hpp"

class ModernValueInput : public Fl_Value_Input {
    Fl_Color borderCol = panelCtrlBorder;

    // Fl_Value_Input honours range() only while dragging; a value the user types
    // in is accepted verbatim. Replace the hidden input's callback with one that
    // clamps the typed value to [minimum(), maximum()] before committing it, so
    // the widget's range is enforced no matter how the value is entered.
    static void clampInput(Fl_Widget*, void* v) {
        auto* self = static_cast<ModernValueInput*>(v);
        double nv;
        if ((self->step() - std::floor(self->step())) > 0.0 || self->step() == 0.0)
            nv = strtod(self->input.value(), nullptr);
        else
            nv = strtol(self->input.value(), nullptr, 0);
        double clamped = self->clamp(nv);
        if (clamped != self->value() || (self->when() & FL_WHEN_NOT_CHANGED)) {
            self->set_value(clamped);
            self->set_changed();
            if (self->when()) self->do_callback(FL_REASON_CHANGED);
        }
        if (clamped != nv) {  // snap the shown text back into range
            char buf[128];
            self->format(buf);
            self->input.value(buf);
        }
    }

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
        input.callback(clampInput, this);
    }

    void setBorderColor(Fl_Color c) { borderCol = c; }
};

#endif
