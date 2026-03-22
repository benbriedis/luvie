#ifndef SONG_EDITOR_HPP
#define SONG_EDITOR_HPP

#include "editor.hpp"
#include "songGrid.hpp"
#include "trackLabels.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include "gridScrollPane.hpp"
#include <vector>

class TrackContextPopup;

class SongEditor : public Editor, public ITimelineObserver {
    static constexpr int labelW    = 80;
    static constexpr int scrollbarW = 14;

    TrackLabels        trackLabels;
    SongGrid           songGrid;
    GridScrollPane*    scrollbar      = nullptr;
    GridScrollPane*    hScrollbar     = nullptr;
    ObservableTimeline* timeline      = nullptr;
    int                numVisibleRows;
    int                rowOffset      = 0;
    int                colOffset      = 0;
    int                baseX          = 0;

    void updateScrollBounds();
    void setRowOffset(int offset);
    void setColOffset(int offset);

public:
    SongEditor(int x, int y, int visibleW, std::vector<Note> notes,
               int numRows, int numCols, int rowHeight, int colWidth,
               float snap, Popup& popup);
    ~SongEditor();

    static constexpr int hScrollH = 14;

    std::function<void(int trackIndex)>        onPatternDoubleClick;
    std::function<void(int offsetX, int clipLeft)> onRulerOffsetChanged;

    void setTransport(ITransport* t, ObservableTimeline* tl);
    void setContextPopup(TrackContextPopup* p);
    void setTrackView(int trackIndex, bool beatResolution);

    void onTimelineChanged() override;
    int  handle(int event) override;
};

#endif
