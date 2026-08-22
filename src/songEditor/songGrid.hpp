// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableSong.hpp"
#include "patternInstanceContextPopup.hpp"
#include "selectionContextPopup.hpp"
#include "paramDotPopup.hpp"
#include "paramLaneTypes.hpp"
#include <cmath>
#include <unordered_set>

// ─────────────────────────────────────────────────────────────────────────────

class SongGrid : public Grid, public ITimelineObserver {
    static constexpr int instrNameRowH = 24;

    bool isInstrHeaderVR(int vr) const;
    ObservableSong* timeline          = nullptr;
    PatternInstanceContextPopup*          songPopup         = nullptr;
    SelectionContextPopup*                selectionPopup    = nullptr;
    ParamDotPopup*      paramDotPopup     = nullptr;
    int                 trackFilter       = -1;
    bool                beatResolution    = false;
    float               tickBarPos        = 0.0f;
    float               dragStartOffset   = 0.0f;
    float               dragBeatsPerBar   = 4.0f;  // the dragged pattern's beats per song bar
    int                 rowOffset         = 0;   // first partially-visible absolute row
    int                 pixelOffset       = 0;   // pixels of rowOffset scrolled off the top

    std::vector<ParamLaneLocal>  localParamLanes;
    ParamState                   paramState;
    std::unordered_set<int>      stackedNoteIds;

    // ── Copy-and-place ───────────────────────────────────────────────────────
    // "Copy selection" leaves a ghost of the copied instances following the
    // cursor; the next click places the copy and ends the gesture, Escape
    // cancels it, and either way the original selection is left as it was. A
    // non-empty `stamp` IS the mode. It holds values rather than ids because
    // the source instances may be edited or deleted while the ghost is up, and
    // what gets placed should stay what was copied.
    struct StampItem { int srcAbsRow; float startBar, length, startOffset; };
    std::vector<StampItem> stamp;
    int   stampOriginX  = 0, stampOriginY = 0;   // grid-relative cursor anchor
    bool  stampAnchored = false;                 // re-anchor on the first move
    float stampDBar     = 0.0f;
    int   stampDRow     = 0;
    // The delta the ghost held when it was last anchored. Re-anchoring (the
    // cursor leaves and comes back, or the view scrolls under it) moves the
    // origin to wherever the cursor now is and carries this forward, so the
    // ghost stays put instead of springing back to the originals.
    float stampBaseDBar = 0.0f;
    int   stampBaseDRow = 0;
    float stampMinDBar  = 0.0f, stampMaxDBar = 0.0f;
    bool  stampBlocked  = false;

    void beginStamp();
    void endStamp();
    void updateStamp();
    // True when every copy would land on a row that holds blocks. The ghost
    // refuses to move onto rows that fail this rather than drawing there.
    bool stampRowsUsable(int dRow) const;
    bool stampIsBlocked() const;
    void commitStamp();
    void drawStamp() const;
    int  handleStampEvent(int event);
    // Destination lane for an absolute row, with the lane's own pattern. Both
    // are -1/0 when the row does not accept instances.
    void destLaneForAbsRow(int absRow, int& laneId, int& patternId) const;
    // The rowOrder index the given lane is drawn on, or -1. Not simply the
    // lane's own RowRef: a stacked track draws all of its lanes on its first
    // lane's row, and only that lane has one.
    int  absRowForLane(int laneId) const;

    // Where a selected instance ends up when the selection moves by some delta.
    struct Landing { int instId; int laneId; float startBar, length; };
    // Fills `out` with one entry per selected instance. False when any of them
    // lands on a row that cannot hold a block — an automation lane, an
    // instrument header, or nothing at all.
    bool collectLandings(float dBeat, int dRow, std::vector<Landing>& out) const;

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
    bool     rowHidesColumnLines(int visualRow) const override { return isInstrHeaderVR(visualRow); }
    Fl_Color rowBgColor(int visualRow) const override;
    void moving(StateDragMove& s) override;
    void resizing(StateDragResize& s) override;
    // A dot dragged along its lane follows the cursor off the edge too, and it
    // runs outside Grid's drag states.
    bool isItemDrag() const override;
    void reapplyDrag() override;
    int  overlappingCell(int noteIdx) const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    void openContextMenu(int idx) override;
    void onBeginDrag(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void onNoteDoubleClick(int noteIdx) override;
    void toggleNote() override;

    std::unordered_set<int> liveItemIds() const override;
    void selectAll() override;
    void deleteSelection() override;
    void groupDragLimits(float& minDBeat, float& maxDBeat,
                         int& minDRow, int& maxDRow) const override;
    bool groupRowsRejected(int dRow) const override;
    bool groupMoveBlocked(float dBeat, int dRow) const override;
    void onCommitGroupMove(float dBeat, int dRow) override;

    // Automation dots share the selection with pattern instances here, because
    // they are rows of this same grid. Both are keyed by their model id, and
    // the id spaces never overlap (one nextId counter issues both).
    void addBandHitExtras() override;
    void previewGroupExtras(float dBeat) override;
    void drawParamSelection(int laneIdx, int rowY) const;
    // Song blocks are placed one whole bar long, at the bar the click lands in;
    // beat subdivisions do not apply here.
    float newNoteLength() const override { return 1.0f; }
    float newNoteStart(float fcol) const override { return std::floor(fcol); }

public:
    SongGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~SongGrid();

    std::function<void(int trackIndex, int laneId)> onPatternDoubleClick;
    std::function<void(int trackIndex, int laneId)> onOpenPattern;

    void setSongPopup(PatternInstanceContextPopup* p)         { songPopup = p; }
    void setSelectionPopup(SelectionContextPopup* p)          { selectionPopup = p; }
    void setParamDotPopup(ParamDotPopup* p) { paramDotPopup = p; }

    int handle(int event) override;

    bool cancelPlacement() override;

    void setTimeline(ObservableSong* tl);
    void setTrackView(int trackFilter, bool beatResolution);
    void setScroll(int rowOff, int pxOff);
    void onTimelineChanged() override;
};

#endif
