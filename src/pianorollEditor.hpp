#ifndef PIANOROLL_EDITOR_HPP
#define PIANOROLL_EDITOR_HPP

#include "editor.hpp"
#include "noteLabelsContextPopup.hpp"
#include "patternParamGrid.hpp"
#include "pianorollGrid.hpp"
#include "paramDotPopup.hpp"
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
    static constexpr int labelsW    = 70;
    static constexpr int scrollbarW = 14;

    GridScrollPane*     scrollbar         = nullptr;
    GridScrollPane*     paramScrollbar    = nullptr;
    PianorollLabels     labels;
    PianorollGrid       grid;
    PatternParamLabels  paramLabels;
    PatternParamGrid    paramGrid;
    ObservableTimeline* timeline          = nullptr;
    int                 lastSelectedTrack = -1;
    int                 colOffset         = 0;
    int                 paramLaneOffset   = 0;

    void setRowOffset(int offset);
    void setColOffset(int offset);
    void relayout();
    void updateParamScrollbar();
    int  handle(int event) override;

public:
    PianorollEditor(int x, int y, int visibleW, int numRows, int numCols,
                    int rowHeight, int colWidth, float snap, Popup& popup);
    ~PianorollEditor();

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    void onTimelineChanged() override;
    void resize(int x, int y, int w, int h) override;
};

#endif
