#ifndef MODERN_SLIDER_HPP
#define MODERN_SLIDER_HPP

#include <FL/Fl.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Group.H>
#include <FL/fl_draw.H>

// A flat, modern horizontal slider: a thin rounded track with a filled
// portion and a circular thumb. Inherits Fl_Slider so value()/bounds()/
// callback()/when() all behave as usual; only the look and the
// mouse-to-value mapping are overridden.
class ModernSlider : public Fl_Slider {
    int      trackH    = 6;
    int      thumbR    = 8;
    bool     hovered   = false;
    bool     pressed   = false;
    Fl_Color trackCol  = 0xE5E7EB00;  // gray-200 (unfilled track)

    double normalized() const {
        double range = maximum() - minimum();
        if (range == 0) return 0;
        double t = (value() - minimum()) / range;
        return t < 0 ? 0 : (t > 1 ? 1 : t);
    }

    static void capsule(int x0, int x1, int cy, int rad) {
        if (x1 > x0)
            fl_rectf(x0, cy - rad, x1 - x0, 2 * rad);
        fl_pie(x0 - rad, cy - rad, 2 * rad, 2 * rad, 0, 360);
        fl_pie(x1 - rad, cy - rad, 2 * rad, 2 * rad, 0, 360);
    }

    void draw() override {
        const int X = x(), Y = y(), W = w(), H = h();
        const int cy = Y + H / 2;
        const int x0 = X + thumbR;
        const int x1 = X + W - thumbR;
        const int thumbX = x0 + (int)(normalized() * (x1 - x0) + 0.5);
        const int rad = trackH / 2;

        // Clear the background to the parent's colour.
        fl_color(parent() ? parent()->color() : FL_WHITE);
        fl_rectf(X, Y, W, H);

        // Unfilled track, then the filled portion up to the thumb.
        fl_color(trackCol);
        capsule(x0, x1, cy, rad);
        fl_color(selection_color());
        capsule(x0, thumbX, cy, rad);

        // Hover/press halo behind the thumb.
        if (hovered || pressed) {
            fl_color(fl_color_average(selection_color(), color(), 0.25f));
            int hr = thumbR + 3;
            fl_pie(thumbX - hr, cy - hr, 2 * hr, 2 * hr, 0, 360);
        }

        // Thumb: accent ring with a white centre.
        fl_color(selection_color());
        fl_pie(thumbX - thumbR, cy - thumbR, 2 * thumbR, 2 * thumbR, 0, 360);
        fl_color(FL_WHITE);
        int ir = thumbR - 2;
        fl_pie(thumbX - ir, cy - ir, 2 * ir, 2 * ir, 0, 360);
    }

    void setFromMouse() {
        const int x0 = x() + thumbR;
        const int x1 = x() + w() - thumbR;
        double t = (x1 > x0) ? double(Fl::event_x() - x0) / (x1 - x0) : 0;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        double v = round(minimum() + t * (maximum() - minimum()));
        if (value(v)) {
            set_changed();
            redraw();
        }
    }

    int handle(int e) override {
        switch (e) {
            case FL_ENTER: hovered = true;  redraw(); return 1;
            case FL_LEAVE: hovered = false; redraw(); return 1;
            case FL_PUSH:
                pressed = true;
                setFromMouse();
                if (when() & FL_WHEN_CHANGED) do_callback();
                return 1;
            case FL_DRAG:
                setFromMouse();
                if (when() & FL_WHEN_CHANGED) do_callback();
                return 1;
            case FL_RELEASE:
                pressed = false;
                redraw();
                if (when() & (FL_WHEN_RELEASE | FL_WHEN_CHANGED)) do_callback();
                return 1;
        }
        return Fl_Slider::handle(e);
    }

public:
    ModernSlider(int x, int y, int w, int h, const char* l = nullptr)
        : Fl_Slider(x, y, w, h, l) {
        box(FL_NO_BOX);
    }

    void setTrackColor(Fl_Color c)  { trackCol = c; }
    void setTrackHeight(int h)      { trackH = h; }
    void setThumbRadius(int r)      { thumbR = r; }
};

#endif
