#ifndef MODERN_BUTTON_HPP
#define MODERN_BUTTON_HPP

#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

class ModernButton : public Fl_Button {
    bool      hovered     = false;
    int       borderWidth = 2;
    Fl_Color  borderCol   = 0xCBD5E100;  // matches transport buttons by default
    Fl_Color  hoverCol    = 0;           // 0 = auto-derive (lighter of bg)

    void draw() override {
        Fl_Color bg = color();
        Fl_Color hov = hoverCol ? hoverCol : fl_color_average(bg, FL_WHITE, 0.8f);
        fl_color(hovered ? hov : bg);
        fl_rectf(x(), y(), w(), h());

        if (borderWidth > 0) {
            fl_color(borderCol);
            fl_line_style(FL_SOLID, borderWidth);
            fl_rect(x(), y(), w(), h());
            fl_line_style(0);
        }

        if (Fl::focus() == this) {
            fl_color(0x3B82F600);
            fl_line_style(FL_SOLID, 2);
            fl_rect(x() + 1, y() + 1, w() - 2, h() - 2);
            fl_line_style(0);
        }

        if (label()) {
            fl_font(labelfont(), labelsize());
            fl_color(active() ? labelcolor() : fl_color_average(labelcolor(), bg, 0.4f));
            fl_draw(label(), x() + 4, y(), w() - 8, h(), align());
        }
    }

    int handle(int event) override {
        if (event == FL_ENTER) { hovered = true;  redraw(); return 1; }
        if (event == FL_LEAVE) { hovered = false; redraw(); return 1; }
        if (event == FL_HIDE)  { hovered = false; return 0; }
        if (event == FL_KEYBOARD) {
            int k = Fl::event_key();
            if (k == FL_Enter || k == FL_KP_Enter) { do_callback(); return 1; }
        }
        return Fl_Button::handle(event);
    }

public:
    ModernButton(int x, int y, int w, int h, const char* label = nullptr)
        : Fl_Button(x, y, w, h, label) { box(FL_NO_BOX); }

    void setBorderWidth(int w)      { borderWidth = w; }
    void setBorderColor(Fl_Color c) { borderCol   = c; }
    void setHoverColor(Fl_Color c)  { hoverCol    = c; }
};

#endif
