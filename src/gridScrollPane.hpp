#ifndef GRID_SCROLL_PANE_HPP
#define GRID_SCROLL_PANE_HPP

#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

class GridScrollPane : public Fl_Widget {
public:
    enum Orientation { VERTICAL, HORIZONTAL };

    GridScrollPane(int x, int y, int w, int h, Orientation ori = VERTICAL)
        : Fl_Widget(x, y, w, h), ori_(ori) {}

    void value(int pos, int sz, int mn, int mx) {
        pos_  = std::clamp(pos, mn, std::max(mn, mx - sz));
        size_ = sz; min_ = mn; max_ = mx;
        redraw();
    }
    int  value()      const { return pos_; }
    void linesize(int s)    { ls_ = s; }

private:
    Orientation ori_;
    int  pos_=0, size_=1, min_=0, max_=1, ls_=1;
    bool dragging_=false, thumbHov_=false;
    int  dragStart_=0, dragStartPos_=0;

    static constexpr int      minThumb    = 30, pad = 2;
    static constexpr Fl_Color trackCol    = 0x1A242E00;
    static constexpr Fl_Color thumbCol    = 0x47556900;
    static constexpr Fl_Color thumbHovCol = 0x64748B00;

    int trackLen() const { return ori_ == VERTICAL ? h() : w(); }

    int thumbLen() const {
        int range = max_ - min_;
        if (range <= 0) return trackLen() - 2*pad;
        int len = (int)((float)size_ / range * (trackLen() - 2*pad));
        return std::max(len, minThumb);
    }

    int thumbPos() const {
        int range = max_ - min_ - size_;
        if (range <= 0) return pad;
        int travel = trackLen() - 2*pad - thumbLen();
        return pad + (int)((float)(pos_ - min_) / range * travel);
    }

    bool overThumb(int ex, int ey) const {
        int tp = thumbPos(), tl = thumbLen();
        if (ori_ == VERTICAL)
            return ey >= y()+tp && ey < y()+tp+tl;
        else
            return ex >= x()+tp && ex < x()+tp+tl;
    }

    void setPos(int p) {
        int clamped = std::clamp(p, min_, std::max(min_, max_ - size_));
        if (clamped != pos_) { pos_ = clamped; do_callback(); redraw(); }
    }

    void draw() override {
        fl_color(trackCol);
        fl_rectf(x(), y(), w(), h());
        fl_color(thumbHov_ ? thumbHovCol : thumbCol);
        int tp = thumbPos(), tl = thumbLen();
        if (ori_ == VERTICAL)
            fl_rectf(x()+pad, y()+tp, w()-2*pad, tl);
        else
            fl_rectf(x()+tp, y()+pad, tl, h()-2*pad);
    }

    int handle(int event) override {
        int coord  = (ori_ == VERTICAL) ? Fl::event_y() : Fl::event_x();
        int origin = (ori_ == VERTICAL) ? y()           : x();
        switch (event) {
        case FL_ENTER: return 1;
        case FL_LEAVE: thumbHov_ = false; redraw(); return 1;
        case FL_MOVE: {
            bool ov = overThumb(Fl::event_x(), Fl::event_y());
            if (ov != thumbHov_) { thumbHov_ = ov; redraw(); }
            return 1;
        }
        case FL_PUSH: {
            if (overThumb(Fl::event_x(), Fl::event_y())) {
                dragging_     = true;
                dragStart_    = coord;
                dragStartPos_ = pos_;
            } else {
                int tp = origin + thumbPos(), tl = thumbLen();
                if (coord < tp)          setPos(pos_ - size_);
                else if (coord >= tp+tl) setPos(pos_ + size_);
            }
            return 1;
        }
        case FL_DRAG: {
            if (dragging_) {
                int travel = trackLen() - 2*pad - thumbLen();
                if (travel <= 0) return 1;
                int range = max_ - min_ - size_;
                if (range <= 0) return 1;
                int delta = (int)((float)(coord - dragStart_) / travel * range);
                setPos(dragStartPos_ + delta);
            }
            return 1;
        }
        case FL_RELEASE: dragging_ = false; return 1;
        }
        return Fl_Widget::handle(event);
    }
};

#endif
