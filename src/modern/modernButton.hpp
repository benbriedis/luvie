// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MODERN_BUTTON_HPP
#define MODERN_BUTTON_HPP

#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include "menuGlyphs.hpp"
#include <functional>

class ModernButton : public Fl_Button {
    bool      hovered     = false;
    int       borderWidth = 2;
    Fl_Color  borderCol   = 0xCBD5E100;  // matches transport buttons by default
    Fl_Color  hoverCol    = 0;           // 0 = auto-derive (lighter of bg)
    bool      arrow       = false;       // trailing "opens a submenu" triangle
    bool      tickSlot    = false;       // leading gutter kept free for a tick
    bool      ticked      = false;       // and a tick drawn in it

public:
    std::function<void()> onEnter;

private:

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

        drawContent(x() + 4, y(), w() - 8, h(), bg);
    }

    int handle(int event) override {
        if (event == FL_ENTER) { hovered = true;  redraw(); if (onEnter) onEnter(); return 1; }
        if (event == FL_LEAVE) { hovered = false; redraw(); return 1; }
        if (event == FL_HIDE)  { hovered = false; return 0; }
        if (event == FL_KEYBOARD) {
            int k = Fl::event_key();
            if (k == FL_Enter || k == FL_KP_Enter) { do_callback(); return 1; }
        }
        return Fl_Button::handle(event);
    }

protected:
    // Renders the button's face inside the box the border leaves. Subclasses whose
    // face is a picture rather than words (see SharpFlatButton) override this. bg
    // is the colour already painted behind it, which an inactive label fades into.
    virtual void drawContent(int X, int Y, int W, int H, Fl_Color bg) {
        Fl_Color col = active() ? labelcolor() : fl_color_average(labelcolor(), bg, 0.4f);
        if (arrow) {
            menuGlyphs::submenuArrow(X + W - 6, Y + H / 2, col);
            W -= 12;
        }
        if (tickSlot) {
            // The gutter is reserved whether or not this row is ticked, so the
            // words stay put as ticks come and go.
            if (ticked) menuGlyphs::tick(X + 7, Y + H / 2, col);
            X += menuGlyphs::tickSlotW;
            W -= menuGlyphs::tickSlotW;
        }
        if (!label()) return;
        fl_font(labelfont(), labelsize());
        fl_color(col);
        fl_draw(label(), X, Y, W, H, align());
    }

public:
    ModernButton(int x, int y, int w, int h, const char* label = nullptr)
        : Fl_Button(x, y, w, h, label) { box(FL_NO_BOX); }

    void setBorderWidth(int w)      { borderWidth = w; }
    void setBorderColor(Fl_Color c) { borderCol   = c; }
    void setHoverColor(Fl_Color c)  { hoverCol    = c; }

    // Menu decorations; see menuGlyphs.hpp for why these are not label text.
    void setSubmenuArrow(bool on) { arrow    = on; redraw(); }
    void reserveTick(bool on)     { tickSlot = on; redraw(); }
    void setTicked(bool on)       { ticked   = on; redraw(); }
};

#endif
