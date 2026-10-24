// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef EDITOR_HPP
#define EDITOR_HPP

#include "playhead.hpp"
#include "loopManager.hpp"
#include "gridScrollPane.hpp"
#include "theme.hpp"
#include <FL/Fl_Group.H>
#include <functional>

inline Fl_Color bgColor() { return theme().editorBg; }

class Editor : public Fl_Group {
public:
    static constexpr int rulerH   = 20;
    static constexpr int hScrollH = 14;

protected:
    static Fl_Color rulerBg()     { return theme().rulerBg; }
    static Fl_Color rulerBorder() { return theme().rulerBorder; }

    Playhead        playhead;
    GridScrollPane* hScrollbar   = nullptr;
    bool            rulerDragging  = false;
    bool            seekingEnabled = true;
    int             rulerOffsetX   = 0;   // x offset of grid area within editor (e.g. label panel width)
    int             hScrollPixel   = 0;   // horizontal scroll offset in pixels
    int             gridPadX       = 0;   // extra left margin before beat 0 inside the grid (drum editor)

    virtual void drawRulerLabels() {}
    void draw() override;
    int  handle(int event) override;

    // Lay the scrolling body out for the current size. Subclasses implement the
    // concrete layout; Editor::resize() and child setup call it.
    virtual void layoutBody() = 0;

    // Mouse-wheel hooks dispatched by Editor::handle(); default to no-ops.
    virtual void onWheelX(int dCols)  {}
    virtual void onWheelY(int dSteps) {}

    Editor(int x, int y, int w, int h, int numCols, int colWidth);

public:
    void resize(int x, int y, int w, int h) override;

    std::function<void()> onEndReached;
    std::function<void()> onSeek;

    void setSeekingEnabled(bool e)                           { seekingEnabled = e; }
    void setPlayheadLoopManager(LoopManager* a)      { playhead.setLoopManager(a); }
    void setPlayheadLoopMode(bool a)                         { playhead.setLoopActive(a); }
    void setPlayheadFrozen(bool f, float bar = 0.0f)         { playhead.setFrozen(f, bar); }
    void setPlayheadHandoff(bool on, float offsetBars = 0.0f) { playhead.setHandoff(on, offsetBars); }
    void setVerbose(bool v)                                  { playhead.setVerbose(v); }
    void setPitchName(std::function<std::string(int)> fn)    { playhead.pitchName = std::move(fn); }
    void setPlayheadSongLoop(std::function<bool(float&,float&)> fn) {
        playhead.songLoopRange = std::move(fn);
    }
};

#endif
