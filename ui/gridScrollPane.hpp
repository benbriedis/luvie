#ifndef GRID_SCROLL_PANE_HPP
#define GRID_SCROLL_PANE_HPP

#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

// Modern-looking vertical scrollbar shared by PatternEditor and SongEditor.
class GridScrollPane : public Fl_Widget {
    int  pos_         = 0;
    int  size_        = 1;
    int  min_         = 0;
    int  max_         = 1;
    int  ls_          = 1;
    bool dragging_    = false;
    int  dragStartY_  = 0;
    int  dragStartPos_= 0;
    bool thumbHov_    = false;

    static constexpr int     minThumbH  = 30;
    static constexpr int     pad        = 2;
    static constexpr Fl_Color trackCol  = 0x1A242E00;
    static constexpr Fl_Color thumbCol  = 0x47556900;
    static constexpr Fl_Color thumbHovCol = 0x64748B00;

    int thumbH() const {
        int range = max_ - min_;
        if (range <= 0) return h();
        float frac = (float)std::max(size_, 1) / (float)range;
        return std::max((int)(frac * h()), minThumbH);
    }

    int thumbY() const {
        int adjRange = (max_ - min_) - size_;
        int travel   = h() - thumbH();
        if (adjRange <= 0 || travel <= 0) return y();
        return y() + (int)((float)(pos_ - min_) / adjRange * travel);
    }

    bool overThumb(int my) const {
        int ty = thumbY(), th = thumbH();
        return my >= ty && my < ty + th;
    }

    void setPos(int p) {
        int adjMax = (max_ - min_) - size_;
        pos_ = std::clamp(p, min_, std::max(min_, adjMax + min_));
        do_callback();
        redraw();
    }

    void draw() override {
        fl_color(trackCol);
        fl_rectf(x(), y(), w(), h());

        int ty = thumbY(), th = thumbH();
        fl_color(thumbHov_ ? thumbHovCol : thumbCol);
        fl_rectf(x() + pad, ty + pad, w() - 2 * pad, th - 2 * pad);
    }

    int handle(int event) override {
        switch (event) {
        case FL_ENTER:
            thumbHov_ = overThumb(Fl::event_y());
            redraw();
            return 1;
        case FL_MOVE: {
            bool now = overThumb(Fl::event_y());
            if (now != thumbHov_) { thumbHov_ = now; redraw(); }
            return 1;
        }
        case FL_LEAVE:
            if (thumbHov_) { thumbHov_ = false; redraw(); }
            return 1;
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                int my = Fl::event_y();
                if (overThumb(my)) {
                    dragging_     = true;
                    dragStartY_   = my;
                    dragStartPos_ = pos_;
                } else {
                    // Click on track above/below thumb: page scroll
                    setPos(my < thumbY() ? pos_ - size_ : pos_ + size_);
                }
                return 1;
            }
            return 0;
        case FL_DRAG:
            if (dragging_) {
                int adjRange = (max_ - min_) - size_;
                int travel   = h() - thumbH();
                int dy       = Fl::event_y() - dragStartY_;
                if (travel > 0 && adjRange > 0)
                    setPos(dragStartPos_ + (int)((float)dy / travel * adjRange));
            }
            return 1;
        case FL_RELEASE:
            dragging_ = false;
            return 1;
        default:
            return 0;
        }
    }

public:
    GridScrollPane(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    // Compatible with Fl_Scrollbar's value(pos, size, min, max) API.
    void value(int pos, int sz, int mn, int mx) {
        pos_  = std::clamp(pos, mn, mx);
        size_ = sz;
        min_  = mn;
        max_  = mx;
        redraw();
    }

    int  value()    const { return pos_; }
    void linesize(int s)  { ls_ = s; }
};

#endif
