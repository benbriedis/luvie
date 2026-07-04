#ifndef EDITOR_HPP
#define EDITOR_HPP

#include "playhead.hpp"
#include "activePatternSet.hpp"
#include "gridScrollPane.hpp"
#include <FL/Fl_Group.H>
#include <functional>

inline constexpr Fl_Color bgColor = FL_WHITE;

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
    void setPlayheadActivePatterns(ActivePatternSet* a)      { playhead.setActivePatterns(a); }
    void setPlayheadLoopMode(bool a)                         { playhead.setLoopActive(a); }
    void setVerbose(bool v)                                  { playhead.setVerbose(v); }
    void setPitchName(std::function<std::string(int)> fn)    { playhead.pitchName = std::move(fn); }
    void setPlayheadPortRegistry(PortRegistry* r)            { playhead.setPortRegistry(r); }
    void setPlayheadHasSoftPorts(bool b)                     { playhead.setHasSoftPorts(b); }
    void setPlayheadHasJackPorts(bool b)                     { playhead.setHasJackPorts(b); }
    void setPlayheadJackClockActive(bool b)                  { playhead.setJackClockActive(b); }
    void playheadPanicSoftNotes()                           { playhead.panicSoftNotes(); }
    void setPlayheadSoftRouting(std::function<int(int,int,int)> r2m,
                                std::function<MidiInstrRoute(int)> ir) {
        playhead.setSoftRouting(std::move(r2m), std::move(ir));
    }
};

#endif
