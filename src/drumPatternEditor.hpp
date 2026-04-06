#ifndef DRUM_PATTERN_EDITOR_HPP
#define DRUM_PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "drumGrid.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include "gridScrollPane.hpp"
#include <FL/Fl_Widget.H>

// Simple left-panel widget that shows MIDI note numbers for each row
class DrumNoteLabels : public Fl_Widget {
    int numRows;
    int rowHeight;
    int rowOffset = 0;

    void draw() override;

public:
    DrumNoteLabels(int x, int y, int w, int numRows, int rowHeight);
    void setRowOffset(int offset) { rowOffset = offset; redraw(); }
    void setNumRows(int n)        { numRows   = n;       redraw(); }
};

// ---------------------------------------------------------------------------

class DrumPatternEditor : public Editor, public ITimelineObserver {
    static constexpr int labelsW    = 36;
    static constexpr int scrollbarW = 14;

    GridScrollPane*     scrollbar         = nullptr;
    DrumNoteLabels      drumLabels;
    DrumGrid            drumGrid;
    ObservableTimeline* timeline          = nullptr;
    int                 lastSelectedTrack = -1;
    int                 colOffset         = 0;

    void setRowOffset(int offset);
    void setColOffset(int offset);
    int  handle(int event) override;

public:
    DrumPatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                      int rowHeight, int colWidth, float snap, Popup& popup);
    ~DrumPatternEditor();

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void onTimelineChanged() override;
    void resize(int x, int y, int w, int h) override;
};

#endif
