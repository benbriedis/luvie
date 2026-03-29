#ifndef EDITOR_HPP
#define EDITOR_HPP

#include "playhead.hpp"
#include "gridScrollPane.hpp"
#include <FL/Fl_Group.H>
#include <functional>

const Fl_Color bgColor = FL_WHITE;

class Editor : public Fl_Group {
public:
    static constexpr int rulerH   = 20;
    static constexpr int hScrollH = 14;

protected:
    static constexpr Fl_Color rulerBg     = 0xFEFCE800;
    static constexpr Fl_Color rulerBorder = 0xD1D5DB00;

    Playhead        playhead;
    GridScrollPane* hScrollbar   = nullptr;
    bool            rulerDragging  = false;
    bool            seekingEnabled = true;
    int             rulerOffsetX   = 0;   // x offset of grid area within editor (e.g. label panel width)
    int             hScrollPixel   = 0;   // horizontal scroll offset in pixels

    virtual void drawRulerLabels() {}
    void draw() override;
    int  handle(int event) override;

    Editor(int x, int y, int w, int h, int numCols, int colWidth);

public:
    std::function<void()> onEndReached;
    std::function<void()> onSeek;

    void setSeekingEnabled(bool e)                           { seekingEnabled      = e; }
    void setVerbose(bool v)                                  { playhead.setVerbose(v); }
    void setPitchName(std::function<std::string(int)> fn)    { playhead.pitchName  = std::move(fn); }
};

#endif
