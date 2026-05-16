#ifndef CURSORS_HPP
#define CURSORS_HPP

#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>

const Fl_RGB_Image* forbiddenCursorImage();
const Fl_RGB_Image* contextMenuCursorImage();

// General hover-cursor helper. Call at the top of a widget's handle().
// Returns >= 0 when the event is consumed by cursor logic (caller should return that value).
// Returns -1 when the event is unrelated to cursor management.
// Covers FL_ENTER (set), FL_LEAVE (clear), FL_DRAG out-of-bounds (clear), FL_RELEASE (clear).
inline int widgetCursorHandle(Fl_Widget* w, int event, const Fl_RGB_Image* img, int hotX = 0, int hotY = 0)
{
    switch (event) {
    case FL_ENTER:
        w->window()->cursor(img, hotX, hotY);
        return 1;
    case FL_LEAVE:
        w->window()->cursor(FL_CURSOR_DEFAULT);
        return 0;
    case FL_DRAG: {
        bool inside = Fl::event_x() >= w->x() && Fl::event_x() < w->x() + w->w()
                   && Fl::event_y() >= w->y() && Fl::event_y() < w->y() + w->h();
        if (!inside) w->window()->cursor(FL_CURSOR_DEFAULT);
        break;
    }
    case FL_RELEASE:
        w->window()->cursor(FL_CURSOR_DEFAULT);
        break;
    }
    return -1;
}

inline int widgetCursorHandle(Fl_Widget* w, int event, Fl_Cursor cursor)
{
    switch (event) {
    case FL_ENTER:
        w->window()->cursor(cursor);
        return 1;
    case FL_LEAVE:
        w->window()->cursor(FL_CURSOR_DEFAULT);
        return 0;
    case FL_DRAG: {
        bool inside = Fl::event_x() >= w->x() && Fl::event_x() < w->x() + w->w()
                   && Fl::event_y() >= w->y() && Fl::event_y() < w->y() + w->h();
        if (!inside) w->window()->cursor(FL_CURSOR_DEFAULT);
        break;
    }
    case FL_RELEASE:
        w->window()->cursor(FL_CURSOR_DEFAULT);
        break;
    }
    return -1;
}

inline int contextMenuCursorHandle(Fl_Widget* w, int event)
{
    return widgetCursorHandle(w, event, contextMenuCursorImage(), 0, 0);
}

#endif
