#ifndef SONG_EDITOR_HPP
#define SONG_EDITOR_HPP

#include "editor.hpp"
#include "gridPane.hpp"
#include "songGrid.hpp"
#include "patternInstanceContextPopup.hpp"
#include "trackLabels.hpp"
#include "noteContextPopup.hpp"
#include "itransport.hpp"
#include "observableSong.hpp"
#include "gridScrollPane.hpp"
#include <vector>

class TrackContextPopup;
class ParamLaneContextPopup;

class SongEditor : public Editor, public ITimelineObserver {
    static constexpr int labelW    = 80;
    static constexpr int controlsW = 22;   // right button column inside the labels strip
    static constexpr int scrollbarW = 14;

    GridPane           gridPane;
    TrackLabels        trackLabels;
    SongGrid           songGrid;
    GridScrollPane*    scrollbar      = nullptr;
    ITransport*        transport      = nullptr;
    ObservableSong*    timeline       = nullptr;
    int                scrollPx       = 0;   // absolute vertical scroll, in pixels
    int                colOffset      = 0;
    int                baseX          = 0;
    bool               wasPlaying     = false;
    bool               pendingScroll  = false;  // snap playhead into view on next tick

    static constexpr int wheelStepPx = 24;   // pixels per mouse-wheel notch

    void updateScrollBounds();
    void setScrollPx(int px);
    void pushScroll(int availH);
    void setColOffset(int offset);
    void drawRulerLabels() override;
    void layoutBody() override { updateScrollBounds(); }
    void onWheelX(int d) override { setColOffset(colOffset + d); }
    void onWheelY(int d) override { setScrollPx(scrollPx + d * wheelStepPx); }
    int  computeNumCols() const;
    void followPlayhead();

public:
    SongEditor(int x, int y, int visibleW,
               int numRows, int numCols, int rowHeight, int colWidth,
               float snap, NoteContextPopup& popup);
    ~SongEditor();

    std::function<void(int trackIndex, int laneId)> onPatternDoubleClick;
    std::function<void(int trackIndex, int laneId)> onOpenPattern;
    std::function<void(int offsetX, int clipLeft)> onRulerOffsetChanged;
    std::function<void(int numCols)>               onNumColsChanged;

    void setSongPopup(PatternInstanceContextPopup* p)         { songGrid.setSongPopup(p); }
    void setParamDotPopup(ParamDotPopup* p) { songGrid.setParamDotPopup(p); }

    void setTransport(ITransport* t, ObservableSong* tl);
    void setPattern(ObservablePattern* p);
    void setContextPopup(TrackContextPopup* p);
    void setParamLaneContextPopup(ParamLaneContextPopup* p);
    void setTrackView(int trackIndex, bool beatResolution);

    // Ask for the playhead to be scrolled to the left edge (if off-screen) on the
    // next timer tick. Deferring to followPlayhead means one scroll update that
    // stays in sync with the playhead redraw, even after an async (JACK) rewind.
    void requestScrollToPlayhead() { pendingScroll = true; }

    void onTimelineChanged() override;
};

#endif
