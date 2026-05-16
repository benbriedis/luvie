#pragma once
#include "observableSong.hpp"
#include "itimelineobserver.hpp"
#include <FL/Fl_Widget.H>

class TrackControls : public Fl_Widget, public ITimelineObserver {
    ObservableSong* timeline       = nullptr;
    int             numVisibleRows;
    int             rowHeight;
    int             rowOffset      = 0;

public:
    TrackControls(int x, int y, int w, int numVisibleRows, int rowHeight);
    ~TrackControls();

    void setTimeline(ObservableSong* tl);
    void setRowOffset(int offset);
    void setNumVisibleRows(int n) { numVisibleRows = n; }
    void onTimelineChanged() override { redraw(); }

protected:
    void draw() override;
    int  handle(int event) override;
};
