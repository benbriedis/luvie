#ifndef OUTER_GRID_HPP
#define OUTER_GRID_HPP

#include "playhead.hpp"
#include <FL/Fl_Group.H>
#include <functional>

const Fl_Color bgColor = FL_WHITE;

class OuterGrid : public Fl_Group {
public:
    static constexpr int rulerH = 20;

protected:
    static constexpr Fl_Color rulerBg     = 0xFEFCE800;
    static constexpr Fl_Color rulerBorder = 0xD1D5DB00;

    Playhead playhead;
    bool     rulerDragging  = false;
    bool     seekingEnabled = true;

    void draw() override;
    int  handle(int event) override;

    OuterGrid(int x, int y, int w, int h, int numCols, int colWidth);

public:
    std::function<void()> onEndReached;
    std::function<void()> onSeek;

    void setSeekingEnabled(bool e) { seekingEnabled = e; }
};

#endif
