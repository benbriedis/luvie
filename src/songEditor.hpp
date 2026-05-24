#ifndef SONG_EDITOR_HPP
#define SONG_EDITOR_HPP

#include "editor.hpp"
#include "songGrid.hpp"
#include "songPopup.hpp"
#include "trackLabels.hpp"
#include "trackControls.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableSong.hpp"
#include "gridScrollPane.hpp"
#include <vector>

class TrackContextPopup;
class ParamLaneContextPopup;

class SongEditor : public Editor, public ITimelineObserver {
    static constexpr int labelW    = 80;
    static constexpr int controlsW = 22;
    static constexpr int scrollbarW = 14;

    TrackLabels        trackLabels;
    TrackControls      trackControls;
    SongGrid           songGrid;
    GridScrollPane*    scrollbar      = nullptr;
    ITransport*        transport      = nullptr;
    ObservableSong*    timeline       = nullptr;
    int                numVisibleRows;
    int                rowOffset      = 0;
    int                colOffset      = 0;
    int                baseX          = 0;
    bool               wasPlaying     = false;

    void updateScrollBounds();
    void setRowOffset(int offset);
    void setColOffset(int offset);
    void drawRulerLabels() override;
    int  computeNumCols() const;
    void followPlayhead();

public:
    SongEditor(int x, int y, int visibleW,
               int numRows, int numCols, int rowHeight, int colWidth,
               float snap, Popup& popup);
    ~SongEditor();

    std::function<void(int trackIndex, int laneId)> onPatternDoubleClick;
    std::function<void(int trackIndex, int laneId)> onOpenPattern;
    std::function<void(int offsetX, int clipLeft)> onRulerOffsetChanged;
    std::function<void(int numCols)>               onNumColsChanged;

    void setSongPopup(SongPopup* p)         { songGrid.setSongPopup(p); }
    void setParamDotPopup(ParamDotPopup* p) { songGrid.setParamDotPopup(p); }

    void setTransport(ITransport* t, ObservableSong* tl);
    void setPattern(ObservablePattern* p);
    void setContextPopup(TrackContextPopup* p);
    void setParamLaneContextPopup(ParamLaneContextPopup* p);
    void setTrackView(int trackIndex, bool beatResolution);

    void onTimelineChanged() override;
    void resize(int x, int y, int w, int h) override;
    int  handle(int event) override;
};

#endif
