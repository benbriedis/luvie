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
    playhead.drawTriangle(x() + rulerOffsetX - hScrollPixel, y(), rulerH);
    draw_children();
}

int Editor::handle(int event)
{
    bool inRuler = Fl::event_y() >= y() && Fl::event_y() < y() + rulerH;
    switch (event) {
    case FL_PUSH:
        if (inRuler && Fl::event_button() == FL_LEFT_MOUSE && seekingEnabled) {
            rulerDragging = true;
            playhead.seek(Fl::event_x(), x() + rulerOffsetX - hScrollPixel);
            if (onSeek) onSeek();
            redraw();
            return 1;
        }
        break;
    case FL_DRAG:
        if (rulerDragging && seekingEnabled) {
            playhead.seek(Fl::event_x(), x() + rulerOffsetX - hScrollPixel);
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
            if (seekingEnabled) {
                const int grabZone = 8;
                int dist = std::abs(Fl::event_x() - (x() + rulerOffsetX - hScrollPixel + playhead.xOffset()));
                window()->cursor(dist <= grabZone ? FL_CURSOR_WE : FL_CURSOR_CROSS);
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
