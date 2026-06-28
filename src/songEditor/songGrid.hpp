#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableSong.hpp"
#include "patternInstanceContextPopup.hpp"
#include "paramDotPopup.hpp"
#include "paramLaneTypes.hpp"
#include <unordered_set>

// ─────────────────────────────────────────────────────────────────────────────

class SongGrid : public Grid, public ITimelineObserver {
    static constexpr int instrNameRowH = 24;

    bool isInstrHeaderVR(int vr) const;
    ObservableSong* timeline          = nullptr;
    PatternInstanceContextPopup*          songPopup         = nullptr;
    ParamDotPopup*      paramDotPopup     = nullptr;
    int                 trackFilter       = -1;
    bool                beatResolution    = false;
    float               tickBarPos        = 0.0f;
    float               dragStartOffset   = 0.0f;
    int                 dragBeatsPerBar   = 4;
    int                 rowOffset         = 0;   // first partially-visible absolute row
    int                 pixelOffset       = 0;   // pixels of rowOffset scrolled off the top

    std::vector<ParamLaneLocal>  localParamLanes;
    ParamState                   paramState;
    std::unordered_set<int>      stackedNoteIds;

    void rebuildNotes();
    void rebuildParamLanes();
    void drawParamRow(int laneIdx, int rowY, int gridRight);
    int  findParamPointAtCursor(int laneIdx) const;
    int  findPrecedingDotIdx(int laneIdx) const;
    bool canPlaceDot(int laneIdx, float beat, int excludeId = -1) const;
    int  handleParamEvent(int event);
    int  visualRowForLaneId(int laneId) const;   // visual (relative) row, or -1
    int  laneIdxForAbsRow(int absRow) const;     // index into localParamLanes, or -1

    int rowY(int r) const override;
    int rowH(int r) const override;
    int rowAtPixelY(int py) const override;
    int gridBottom() const override { return totalPixelH(); }
    int absRowHeight(int absRow) const;   // pixel height of an absolute rowOrder entry

public:
    int totalPixelH() const;
    int fullContentHeight() const;             // total pixel height of every row
    // Map an absolute pixel scroll position to (first visible row, pixels into it).
    void scrollPxToRow(int scrollPx, int& rowOff, int& pxOff) const;
    // Number of rows to render to cover availH starting at rowOff with pxOff scrolled off.
    int  rowsToRender(int rowOff, int pxOff, int availH) const;

protected:
    void draw() override;
    // Song blocks are not velocity-coloured — keep a fixed block colour.
    void drawNoteBlock(const Note& note, int x0, int y0, int width, int rh) override;
    bool     isRowBlocked(int visualRow) const override;
    Fl_Color rowBgColor(int visualRow) const override;
    void moving(StateDragMove& s) override;
    void resizing(StateDragResize& s) override;
    int  overlappingCell(int noteIdx) const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    void openContextMenu(int idx) override;
    void onBeginDrag(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void onNoteDoubleClick(int noteIdx) override;
    void toggleNote() override;

public:
    SongGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~SongGrid();

    std::function<void(int trackIndex, int laneId)> onPatternDoubleClick;
    std::function<void(int trackIndex, int laneId)> onOpenPattern;

    void setSongPopup(PatternInstanceContextPopup* p)         { songPopup = p; }
    void setParamDotPopup(ParamDotPopup* p) { paramDotPopup = p; }

    int handle(int event) override;

    void setTimeline(ObservableSong* tl);
    void setTrackView(int trackFilter, bool beatResolution);
    void setScroll(int rowOff, int pxOff);
    void onTimelineChanged() override;
};

#endif
