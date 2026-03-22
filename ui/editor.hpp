#ifndef EDITOR_HPP
#define EDITOR_HPP

#include "playhead.hpp"
#include <FL/Fl_Group.H>
#include <functional>

const Fl_Color bgColor = FL_WHITE;

class Editor : public Fl_Group {
public:
    static constexpr int rulerH = 20;

protected:
    static constexpr Fl_Color rulerBg     = 0xFEFCE800;
    static constexpr Fl_Color rulerBorder = 0xD1D5DB00;

    Playhead playhead;
    bool     rulerDragging  = false;
    bool     seekingEnabled = true;
    int      rulerOffsetX   = 0;   // x offset of grid area within editor (e.g. label panel width)
    int      hScrollPixel   = 0;   // horizontal scroll offset in pixels

    void draw() override;
    int  handle(int event) override;

    Editor(int x, int y, int w, int h, int numCols, int colWidth);

public:
    std::function<void()> onEndReached;
    std::function<void()> onSeek;

    void setSeekingEnabled(bool e) { seekingEnabled = e; }
};

#endif
