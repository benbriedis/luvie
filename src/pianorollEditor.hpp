#ifndef PIANOROLL_EDITOR_HPP
#define PIANOROLL_EDITOR_HPP

#include "editor.hpp"
#include "noteLabelsContextPopup.hpp"
#include "pianorollGrid.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include "gridScrollPane.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

// Left-panel widget showing MIDI note names (C0, C#0, …) for each row
class PianorollLabels : public Fl_Widget {
    int numRows;
    int rowHeight;
    int rowOffset = 0;

    void draw() override;
    int  handle(int event) override;

public:
    std::function<void()> onRightClick;

    PianorollLabels(int x, int y, int w, int numRows, int rowHeight);
    void setRowOffset(int offset) { rowOffset = offset; redraw(); }
    void setNumRows(int n)        { numRows   = n;       redraw(); }
};

// ---------------------------------------------------------------------------

class PianorollEditor : public Editor, public ITimelineObserver {
    static constexpr int labelsW    = 42;
    static constexpr int scrollbarW = 14;

    GridScrollPane*     scrollbar         = nullptr;
    PianorollLabels     labels;
    PianorollGrid       grid;
    ObservableTimeline* timeline          = nullptr;
    int                 lastSelectedTrack = -1;
    int                 colOffset         = 0;

    void setRowOffset(int offset);
    void setColOffset(int offset);
    int  handle(int event) override;

public:
    PianorollEditor(int x, int y, int visibleW, int numRows, int numCols,
                    int rowHeight, int colWidth, float snap, Popup& popup);
    ~PianorollEditor();

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void onTimelineChanged() override;
    void resize(int x, int y, int w, int h) override;
};

#endif
