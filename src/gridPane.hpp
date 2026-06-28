#ifndef GRID_PANE_HPP
#define GRID_PANE_HPP

#include <FL/Fl_Group.H>

// A plain container for the scrolling editor body — the grid plus its vertical
// and horizontal scrollbars (and the song editor's label/control panels). The
// owning editor lays the children out manually with absolute coordinates, so
// this group must not reflow them: resize() only moves/sizes the group itself.
class GridPane : public Fl_Group {
public:
    GridPane(int x, int y, int w, int h) : Fl_Group(x, y, w, h)
    {
        box(FL_NO_BOX);   // children fully cover the pane
        end();
    }

    void resize(int x, int y, int w, int h) override
    {
        Fl_Widget::resize(x, y, w, h);   // do not reflow children
    }
};

#endif
