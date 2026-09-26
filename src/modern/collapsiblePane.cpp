// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "collapsiblePane.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

static constexpr int kTitlePad   = 16;   // matches OverlayWindow::titlePad
static constexpr int kChevronW   = 10;
static constexpr int kChevronGap = 8;
static constexpr Fl_Color kFocusCol = 0x3B82F600;

CollapsiblePane::CollapsiblePane(int x, int y, int w, const char* title)
    : Fl_Group(x, y, w, kHeaderH), title_(title ? title : "")
{
    box(FL_NO_BOX);      // draw() paints the background itself
    resizable(nullptr);  // move the body with the pane; never scale it
    end();
}

void CollapsiblePane::setExpanded(bool on)
{
    if (on == expanded_) return;
    expanded_ = on;
    for (int i = 0; i < children(); i++) {
        if (on) child(i)->show();
        else    child(i)->hide();
    }
    // Plain Fl_Widget::size: the children stay where they are.
    Fl_Widget::size(w(), kHeaderH + (on ? bodyH_ : 0));
    redraw();
}

void CollapsiblePane::setBodyHeight(int h)
{
    bodyH_ = h;
    Fl_Widget::size(w(), kHeaderH + (expanded_ ? bodyH_ : 0));
}

void CollapsiblePane::setSummary(std::string s, Fl_Color color)
{
    summary_    = std::move(s);
    summaryCol_ = color;
    redraw();
}

void CollapsiblePane::setColors(Fl_Color bg, Fl_Color text, Fl_Color subText, Fl_Color rule)
{
    bgCol_ = bg; textCol_ = text; subTextCol_ = subText; ruleCol_ = rule;
    summaryDefaultCol_ = summaryCol_ = subText;
    redraw();
}

void CollapsiblePane::toggle()
{
    // Taking focus first sends FL_UNFOCUS to whatever had it, so an edit in a
    // body field about to be hidden is committed rather than left pending.
    Fl::focus(this);
    setExpanded(!expanded_);
    do_callback();
}

bool CollapsiblePane::inHeader() const
{
    return Fl::event_inside(x(), y(), w(), kHeaderH);
}

// A child going in while the pane is folded would otherwise show through it.
int CollapsiblePane::on_insert(Fl_Widget* w, int index)
{
    if (!expanded_) w->hide();
    return Fl_Group::on_insert(w, index);
}

int CollapsiblePane::handle(int event)
{
    switch (event) {
    case FL_PUSH:
        if (inHeader() && Fl::event_button() == FL_LEFT_MOUSE) { toggle(); return 1; }
        break;
    case FL_ENTER:
    case FL_MOVE: {
        const bool h = inHeader();
        if (h != hover_) { hover_ = h; redraw(); }
        if (h) return 1;
        break;
    }
    case FL_LEAVE:
        if (hover_) { hover_ = false; redraw(); }
        break;
    case FL_FOCUS:
    case FL_UNFOCUS:
        // Only the pane itself is focused here — a child's own focus events go
        // straight to the child — so this is the heading gaining or losing it.
        if (Fl::focus() == this || event == FL_UNFOCUS) { redraw(); return 1; }
        break;
    case FL_KEYBOARD:
        if (Fl::focus() == this && Fl::event_key() == ' ') { toggle(); return 1; }
        break;
    default:
        break;
    }
    return Fl_Group::handle(event);
}

void CollapsiblePane::draw()
{
    // Only children damaged: leave our own pixels alone and just update them.
    if (!(damage() & ~FL_DAMAGE_CHILD)) {
        if (expanded_) draw_children();
        return;
    }

    fl_color(bgCol_);
    fl_rectf(x(), y(), w(), h());

    if (hover_) {
        fl_color(fl_color_average(ruleCol_, bgCol_, 0.35f));
        fl_rectf(x(), y() + 1, w(), kHeaderH - 1);
    }

    fl_color(ruleCol_);
    fl_line_style(FL_SOLID, 1);
    fl_line(x(), y(), x() + w() - 1, y());
    fl_line_style(0);

    // Chevron: pointing right when folded, down when open.
    const int cx = x() + kTitlePad + kChevronW / 2;
    const int cy = y() + 12 + (kHeaderH - 12) / 2;   // level with the title
    fl_color(subTextCol_);
    if (expanded_)
        fl_polygon(cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 3);
    else
        fl_polygon(cx - 3, cy - 5, cx - 3, cy + 5, cx + 3, cy);

    const int tx = x() + kTitlePad + kChevronW + kChevronGap;
    fl_font(FL_HELVETICA_BOLD, 13);
    fl_color(textCol_);
    fl_draw(title_.c_str(), tx, y() + 12, w() - tx - kTitlePad, kHeaderH - 12,
            FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    if (!summary_.empty()) {
        const int sx = tx + (int)fl_width(title_.c_str()) + 12;
        fl_font(FL_HELVETICA, 11);
        fl_color(summaryCol_);
        fl_draw(summary_.c_str(), sx, y() + 12, x() + w() - sx - kTitlePad, kHeaderH - 12,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    }

    if (Fl::focus() == this) {
        fl_color(kFocusCol);
        fl_line_style(FL_DOT, 1);
        fl_rect(x() + 4, y() + 6, w() - 8, kHeaderH - 8);
        fl_line_style(0);
    }

    if (expanded_) {
        if (drawBody) drawBody();
        draw_children();
    }
}
