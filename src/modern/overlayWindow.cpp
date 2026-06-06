#include "overlayWindow.hpp"
#include "modernButton.hpp"
#include "gridScrollPane.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <algorithm>

OverlayWindow::OverlayWindow(int x, int y, int w, int h, std::string title)
    : BasePopup(x, y, w, h), title_(std::move(title))
{
    box(FL_NO_BOX);
    begin();

    closeBtn_ = new ModernButton(
        w - closePad - closeSz, (headerH - closeSz) / 2,
        closeSz, closeSz, "\xc3\x97");
    closeBtn_->labelsize(22);
    closeBtn_->labelcolor(textCol);
    closeBtn_->color(bgCol);
    closeBtn_->setBorderWidth(0);
    closeBtn_->callback([](Fl_Widget*, void* d) {
        static_cast<OverlayWindow*>(d)->hide();
    }, this);

    vScrollbar_ = new GridScrollPane(w - scrollbarW, headerH, scrollbarW, h - headerH);
    vScrollbar_->linesize(20);
    vScrollbar_->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<OverlayWindow*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->applyScrollY((int)sb->value());
    }, this);

    end();

    // No content yet — hide the scrollbar until a subclass reports content.
    updateScrollbar();
}

void OverlayWindow::hide() {
    if (onClose) onClose();
    BasePopup::hide();
}

void OverlayWindow::resize(int x, int y, int w, int h) {
    Fl_Window::resize(x, y, w, h);
    if (closeBtn_)   closeBtn_->position(w - closePad - closeSz, (headerH - closeSz) / 2);
    if (vScrollbar_) vScrollbar_->resize(w - scrollbarW, headerH, scrollbarW, h - headerH);
    onResized();
}

void OverlayWindow::updateScrollbar() {
    if (!vScrollbar_) return;
    const int available = h() - headerH;
    const int maxScroll = std::max(0, totalContentH_ - available);
    if (maxScroll <= 0) {
        scrollY_ = 0;
        vScrollbar_->hide();
    } else {
        scrollY_ = std::clamp(scrollY_, 0, maxScroll);
        vScrollbar_->value(scrollY_, available, 0, totalContentH_);
        vScrollbar_->show();
    }
}

void OverlayWindow::applyScrollY(int newY) {
    if (newY == scrollY_) return;
    const int delta = scrollY_ - newY;
    scrollY_ = newY;
    onScroll(delta);
    if (vScrollbar_) vScrollbar_->value(scrollY_, h() - headerH, 0, totalContentH_);
    redraw();
}

int OverlayWindow::handle(int event) {
    if (event == FL_MOUSEWHEEL && vScrollbar_ && vScrollbar_->visible()) {
        const int available = h() - headerH;
        const int maxScroll = std::max(0, totalContentH_ - available);
        const int newY = std::clamp(scrollY_ + Fl::event_dy() * 20, 0, maxScroll);
        applyScrollY(newY);
        return 1;
    }
    return BasePopup::handle(event);
}

void OverlayWindow::draw() {
    const bool full = damage() & ~FL_DAMAGE_CHILD;
    const int  sy   = scrollY_;
    const int  sbW  = (vScrollbar_ && vScrollbar_->visible()) ? scrollbarW : 0;

    if (full) {
        // Background + border
        fl_color(bgCol);
        fl_rectf(0, 0, w(), h());

        fl_color(borderCol);
        fl_line_style(FL_SOLID, 1);
        fl_rect(0, 0, w(), h());
        fl_line_style(0);

        // Header divider + title
        fl_color(dividerCol);
        fl_line_style(FL_SOLID, 1);
        fl_line(0, headerH, w(), headerH);
        fl_line_style(0);

        fl_font(FL_HELVETICA_BOLD, 17);
        fl_color(textCol);
        fl_draw(title_.c_str(), titlePad, 0, w() - 2*titlePad, headerH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        // Subclass static content — clipped to content area
        fl_push_clip(0, headerH, w() - sbW, h() - headerH);
        drawStaticContent(sy, sbW);
        fl_pop_clip();
    }

    // Fixed header child (close button) — no content-area clip
    if (full) draw_child(*closeBtn_);
    else      update_child(*closeBtn_);

    // Content children clipped to content area
    fl_push_clip(0, headerH, w() - sbW, h() - headerH);
    for (int i = 0; i < children(); i++) {
        Fl_Widget* c = child(i);
        if (c == closeBtn_ || c == vScrollbar_) continue;
        if (full) draw_child(*c);
        else      update_child(*c);
    }
    fl_pop_clip();

    // Scrollbar without content-area clip restriction
    if (vScrollbar_ && vScrollbar_->visible()) {
        if (full) draw_child(*vScrollbar_);
        else      update_child(*vScrollbar_);
    }
}
