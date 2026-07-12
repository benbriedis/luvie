#ifndef BASE_PATTERN_EDITOR_HPP
#define BASE_PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "gridPane.hpp"
#include "patternParamGrid.hpp"
#include "noteLabelsContextPopup.hpp"
#include "paramDotPopup.hpp"
#include "itransport.hpp"
#include "observablePattern.hpp"
#include "gridScrollPane.hpp"
#include <functional>

class NoteAuditioner;

class BasePatternEditor : public Editor, public ITimelineObserver {
protected:
    static constexpr int scrollbarW = 14;

    GridPane           gridPane;
    GridScrollPane*    scrollbar      = nullptr;
    GridScrollPane*    paramScrollbar = nullptr;
    PatternParamLabels paramLabels;
    PatternParamGrid   paramGrid;
    ObservablePattern* pattern             = nullptr;
    NoteAuditioner*    auditioner          = nullptr;
    int                lastSelectedTrack  = -1;
    int                lastSelectedLaneId = -1;
    int                lastPatId          = -1;
    int                colOffset         = 0;
    int                paramLaneOffset   = 0;
    int                baseColWidth      = 0;   // colWidth at zoom x1
    float              lastLengthBeats   = -1.0f;

    // Subclass grid geometry — all are one-liners forwarding to the concrete grid/labels
    virtual int  labelsWidth()      const = 0;
    virtual int  totalRows()        const = 0;
    virtual int  gridNumRows()      const = 0;
    virtual int  gridNumCols()      const = 0;
    virtual int  gridRowHeight()    const = 0;
    virtual int  gridColWidth()     const = 0;
    virtual int  gridWidgetW()      const = 0;
    virtual int  currentRowOffset() const = 0;  // from labels (PatternEditor) or grid (others)
    virtual void gridSetRowOffset(int offset)             = 0;
    virtual void gridSetColOffset(int offset)             = 0;
    virtual void gridSetColWidth(int colWidth)            = 0;
    virtual void gridSetNumRows(int n)                    = 0;
    virtual void gridSetNumCols(int n)                    = 0;
    virtual void gridResize(int x, int y, int w, int h)   = 0;
    virtual void labelsSetRowOffset(int offset)           = 0;
    virtual void labelsSetNumRows(int n)                  = 0;
    virtual void labelsResize(int x, int y, int w, int h) = 0;
    virtual void labelsSetOnRightClick(std::function<void()> fn) = 0;
    virtual void labelsSetOnRowClicked(std::function<void(int midi)> fn) = 0;

    // Instrument of the currently selected track's pattern (0 if none).
    int currentInstrumentId() const;

    // onTimelineChanged skeleton hooks
    virtual void setGridPattern(int patId)    = 0;
    virtual void afterTimelineChanged(int /*patId*/) {}

    void setRowOffset(int offset);
    void setColOffset(int offset);
    void applyPatternLength(int patId);
    void updateParamScrollbar();
    void relayout();
    void layoutBody() override { relayout(); }
    void onWheelX(int d) override { setColOffset(colOffset + d); }
    void onWheelY(int d) override { setRowOffset(currentRowOffset() - d); }

    BasePatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                      int rowHeight, int colWidth, float snap, int lw);

public:
    ~BasePatternEditor();

    virtual void focusPattern() {}
    virtual void setSnap(float s) { paramGrid.setSnap(s); }
    // Beat subdivisions (1 = None): drawn as faint grid lines, independent of snapping.
    virtual void setDivisions(int d) { (void)d; }
    // Zoom factor (1/2/4): scales column width from its x1 base; note minimum
    // pixel widths are unaffected, so shorter notes remain creatable when zoomed.
    void setZoom(int factor);
    void setPatternPlayhead(ITransport* t, ObservablePattern* pat, int trackIndex);
    void setAuditioner(NoteAuditioner* a);
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    void onTimelineChanged() override;
};

#endif
