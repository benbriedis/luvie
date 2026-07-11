#ifndef MODERN_SPINNER_HPP
#define MODERN_SPINNER_HPP

#include <FL/Fl.H>
#include <FL/Fl_Spinner.H>
#include <FL/fl_draw.H>
#include <cmath>
#include "panelStyle.hpp"

// A dark-themed Fl_Spinner styled to match the panel text boxes:
//  - flat input field + flat up/down buttons, with a thin surrounding border
//  - the mouse wheel over the widget nudges the value by one step
//  - click-and-drag up/down anywhere on the input field changes the value
//    (a plain click without a drag just focuses the field for typing)
//
// Everything else (value()/range()/step()/format()/callback()/when()) behaves
// exactly like Fl_Spinner, so callers configure it the same way.
class ModernSpinner : public Fl_Spinner {
    Fl_Color borderCol = panelCtrlBorder;

    // Drag-to-change state. dragArmed is set on a press inside the input field;
    // dragging flips true once the pointer moves past the threshold.
    bool   dragArmed  = false;
    bool   dragging   = false;
    int    dragStartY = 0;
    double dragStartValue = 0;

    static constexpr int dragThreshold  = 3;  // px before a press counts as a drag
    static constexpr int pixelsPerStep  = 5;  // vertical px that equal one step

    // Clamp v to [minimum, maximum]; if wrap() is on, roll past a bound to the
    // opposite one (matching Fl_Spinner's own button behaviour).
    double clampWrap(double v) const {
        if (v > maximum()) return wrap() ? minimum() : maximum();
        if (v < minimum()) return wrap() ? maximum() : minimum();
        return v;
    }

    void applyValue(double v) {
        if (v == value()) return;
        value(v);
        set_changed();
        do_callback(FL_REASON_CHANGED);
        redraw();
    }

    // Nudge by a whole number of steps (wheel / keyboard-style change).
    void bumpSteps(int steps) {
        applyValue(clampWrap(value() + steps * step()));
    }

public:
    ModernSpinner(int x, int y, int w, int h, const char* l = nullptr)
        : Fl_Spinner(x, y, w, h, l) {
        box(FL_FLAT_BOX);
        color(0x37415100);            // input-field background
        labelcolor(panelText);        // up/down arrow colour
        textcolor(panelText);

        input_.box(FL_FLAT_BOX);
        input_.cursor_color(panelText);
        input_.selection_color(panelCtrlBorder);

        up_button_.box(FL_FLAT_BOX);
        down_button_.box(FL_FLAT_BOX);
        up_button_.color(0x45506300);
        down_button_.color(0x45506300);
        up_button_.selection_color(0x5B637300);
        down_button_.selection_color(0x5B637300);
    }

    void setBorderColor(Fl_Color c) { borderCol = c; }

    void draw() override {
        Fl_Spinner::draw();
        // Faint divider between the input field and the button column.
        fl_color(borderCol);
        fl_line(up_button_.x(), y() + 1, up_button_.x(), y() + h() - 2);
    }

    int handle(int event) override {
        switch (event) {
            case FL_MOUSEWHEEL: {
                int dy = Fl::event_dy();
                if (dy != 0) { bumpSteps(-dy); return 1; }
                return 0;
            }

            case FL_PUSH:
                // Let the up/down buttons work as usual.
                if (Fl::event_inside(&up_button_) || Fl::event_inside(&down_button_))
                    break;
                if (Fl::event_button() == FL_LEFT_MOUSE && Fl::event_inside(&input_)) {
                    dragArmed  = true;
                    dragging   = false;
                    dragStartY = Fl::event_y();
                    dragStartValue = value();
                    return 1;  // become the pushed widget so drags reach us
                }
                break;

            case FL_DRAG:
                if (dragArmed) {
                    int dy = dragStartY - Fl::event_y();  // up = increase
                    if (!dragging && std::abs(dy) > dragThreshold) {
                        dragging = true;
                        if (window()) window()->cursor(FL_CURSOR_NS);
                    }
                    if (dragging) {
                        long steps = std::lround((double)dy / pixelsPerStep);
                        applyValue(clampWrap(dragStartValue + steps * step()));
                    }
                    return 1;
                }
                break;

            case FL_RELEASE:
                if (dragArmed) {
                    bool wasDrag = dragging;
                    dragArmed = dragging = false;
                    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
                    if (!wasDrag)
                        input_.take_focus();  // plain click: focus for typing
                    return 1;
                }
                break;
        }
        return Fl_Spinner::handle(event);
    }
};

#endif
