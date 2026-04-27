#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"
#include "songPopup.hpp"
#include <variant>

// ── Local param lane data (for rendering / drag preview) ─────────────────────
struct ParamPtLocal {
    int   id;
    float beat;
    int   value;
    bool  anchor;
};
struct ParamLaneLocal {
    int                    id;
    std::vector<ParamPtLocal> points;  // sorted by beat
};

struct ParamIdle {};
struct ParamPendingCreate { int laneIdx; float beat; int value; };
struct ParamDragState     { int laneIdx, ptIdx; float origBeat; int origValue; bool moved = false; };
struct ParamVirtualDrag   { int laneIdx, predPtIdx, origValue; bool moved = false; };
using ParamState = std::variant<ParamIdle, ParamPendingCreate, ParamDragState, ParamVirtualDrag>;

// ─────────────────────────────────────────────────────────────────────────────

class SongGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    SongPopup*          songPopup         = nullptr;
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

    void setSongPopup(SongPopup* p) { songPopup = p; }

    int handle(int event) override;

    void setTimeline(ObservableTimeline* tl);
    void setTrackView(int trackFilter, bool beatResolution);
    void setRowOffset(int offset);
    void onTimelineChanged() override;
};

#endif
