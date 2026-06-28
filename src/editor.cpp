#include "editor.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <cstdlib>

Editor::Editor(int x, int y, int w, int h, int numCols, int colWidth)
    : Fl_Group(x, y, w, h),
      playhead(numCols, colWidth)
{}

void Editor::draw()
{
    fl_color(rulerBg);
    fl_rectf(x(), y(), w(), rulerH);
    fl_color(rulerBorder);
    fl_line(x(), y() + rulerH - 1, x() + w() - 1, y() + rulerH - 1);
    // Fill the body so any space below the content (between a short grid and the
    // bottom of the editor) renders as clean white space rather than stale pixels.
    fl_color(bgColor);
    fl_rectf(x(), y() + rulerH, w(), h() - rulerH);
    drawRulerLabels();
    playhead.drawTriangle(x() + rulerOffsetX + gridPadX - hScrollPixel, y(), rulerH);
    draw_children();
}

void Editor::resize(int x, int /*y*/, int w, int h)
{
    Fl_Widget::resize(x, y(), w, h);   // top is pinned; subclass lays out the body
    layoutBody();
}

int Editor::handle(int event)
{
    bool inRuler     = Fl::event_y() >= y() && Fl::event_y() < y() + rulerH;
    bool inGridRuler = inRuler && Fl::event_x() >= x() + rulerOffsetX;
    switch (event) {
    case FL_MOUSEWHEEL:
        if (Fl::event_dx() != 0) onWheelX(Fl::event_dx());
        else                     onWheelY(Fl::event_dy());
        return 1;
    case FL_PUSH:
        if (inGridRuler && Fl::event_button() == FL_LEFT_MOUSE && seekingEnabled) {
            rulerDragging = true;
            playhead.seek(Fl::event_x(), x() + rulerOffsetX + gridPadX - hScrollPixel);
            if (onSeek) onSeek();
            redraw();
            return 1;
        }
        break;
    case FL_DRAG:
        if (rulerDragging && seekingEnabled) {
            playhead.seek(Fl::event_x(), x() + rulerOffsetX + gridPadX - hScrollPixel);
            if (onSeek) onSeek();
            redraw();
            return 1;
        }
        break;
    case FL_RELEASE:
        if (rulerDragging) { rulerDragging = false; return 1; }
        break;
    case FL_MOVE:
        if (inRuler) {
            if (seekingEnabled && inGridRuler) {
                const int grabZone = 8;
                int dist = std::abs(Fl::event_x() - (x() + rulerOffsetX + gridPadX - hScrollPixel + playhead.xOffset()));
                window()->cursor(dist <= grabZone ? FL_CURSOR_WE : FL_CURSOR_CROSS);
            } else {
                window()->cursor(FL_CURSOR_DEFAULT);
            }
            return 1;
        }
        break;
    case FL_ENTER:
        return 1;
    case FL_LEAVE:
        window()->cursor(FL_CURSOR_DEFAULT);
        return 0;
    }
    return Fl_Group::handle(event);
}
