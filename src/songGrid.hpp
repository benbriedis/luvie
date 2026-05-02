#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"
#include "songPopup.hpp"
#include "paramDotPopup.hpp"
#include "paramLaneTypes.hpp"

// ─────────────────────────────────────────────────────────────────────────────

class SongGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    SongPopup*          songPopup         = nullptr;
    ParamDotPopup*      paramDotPopup     = nullptr;
    int                 trackFilter       = -1;
    bool                beatResolution    = false;
    float               tickBarPos        = 0.0f;
    float               dragStartOffset   = 0.0f;
    int                 dragBeatsPerBar   = 4;
    int                 rowOffset         = 0;

    std::vector<ParamLaneLocal> localParamLanes;
    ParamState                  paramState;

    void rebuildNotes();
    void rebuildParamLanes();
    void drawParamRow(int laneIdx, int rowY, int gridRight);
    int  findParamPointAtCursor(int laneIdx) const;
    int  findPrecedingDotIdx(int laneIdx) const;
    bool canPlaceDot(int laneIdx, float beat, int excludeId = -1) const;
    int  handleParamEvent(int event);
    int  visualRowForLaneId(int laneId) const;   // visual (relative) row, or -1
    int  laneIdxForAbsRow(int absRow) const;     // index into localParamLanes, or -1

protected:
    void draw() override;
    Fl_Color rowBgColor(int visualRow) const override;
    void resizing(StateDragResize& s) override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    void openContextMenu(int idx) override;
    void onBeginDrag(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void onNoteDoubleClick(int noteIdx) override;
    void toggleNote() override;

public:
    SongGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup);
    ~SongGrid();

    std::function<void(int trackIndex)> onPatternDoubleClick;
    std::function<void(int trackIndex)> onOpenPattern;

    void setSongPopup(SongPopup* p)         { songPopup = p; }
    void setParamDotPopup(ParamDotPopup* p) { paramDotPopup = p; }

    int handle(int event) override;

    void setTimeline(ObservableTimeline* tl);
    void setTrackView(int trackFilter, bool beatResolution);
    void setRowOffset(int offset);
    void onTimelineChanged() override;
};

#endif
