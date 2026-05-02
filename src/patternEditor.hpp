#ifndef PATTERN_EDITOR_HPP
#define PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "patternGrid.hpp"
#include "patternParamGrid.hpp"
#include "noteLabels.hpp"
#include "noteLabelsContextPopup.hpp"
#include "paramDotPopup.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include "gridScrollPane.hpp"
#include <vector>

class PatternEditor : public Editor, public ITimelineObserver {
    static constexpr int labelsW    = 70;
    static constexpr int scrollbarW = 14;

    GridScrollPane*     scrollbar         = nullptr;
    GridScrollPane*     paramScrollbar    = nullptr;
    NoteLabels          noteLabels;
    PatternGrid         patternGrid;
    PatternParamLabels  paramLabels;
    PatternParamGrid    paramGrid;
    ObservableTimeline* timeline          = nullptr;
    int                 lastSelectedTrack = -1;
    float               lastLengthBeats   = -1.0f;
    int                 rootPitch         = 0;
    int                 chordType         = 0;
    int                 colOffset         = 0;
    int                 paramLaneOffset   = 0;

    int  computeDefaultOffset(int patId) const;
    void setRowOffset(int offset);
    void setColOffset(int offset);
    void focusPattern();
    void updateParamScrollbar();
    int  handle(int event) override;

public:
    PatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, Popup& popup);
    ~PatternEditor();

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setNoteParams(int rootPitch, int chordType, bool useSharp);
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    int  numPatternBeats() const { return patternGrid.numCols; }
    void onTimelineChanged() override;
    void resize(int x, int y, int w, int h) override;
};

#endif
