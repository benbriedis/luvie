// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PIANOROLL_GRID_HPP
#define PIANOROLL_GRID_HPP

#include "grid.hpp"
#include "observablePattern.hpp"

class PianorollGrid : public Grid, public ITimelineObserver {
    ObservablePattern* pattern  = nullptr;
    int                 patternId = -1;
    int                 rowOffset = 0;

    void rebuildNotes();

protected:
    Fl_Color columnColor(int col) const override;
    Fl_Color rowLineColor(int i)  const override;
    Fl_Color rowBgColor(int row)  const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    std::function<void(float)> makeVelocityCallback(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void toggleNote() override;

    // Selection works off the pattern, not the visible rows, so it survives
    // scrolling and ctrl-A reaches notes above and below the viewport.
    std::unordered_set<int> liveItemIds() const override;
    void selectAll() override;
    void deleteSelection() override;
    void groupDragLimits(float& minDBeat, float& maxDBeat,
                         int& minDRow, int& maxDRow) const override;
    bool groupMoveBlocked(float dBeat, int dRow) const override;
    void onCommitGroupMove(float dBeat, int dRow) override;

public:
    static constexpr int totalRows = 128;

    PianorollGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~PianorollGrid();

    void setPattern(ObservablePattern* tl, int patId);
    void setRowOffset(int offset);
    void setNumRows(int n) { numRows = n; rebuildNotes(); }
    void onTimelineChanged() override;

    int getRowOffset() const { return rowOffset; }
};

#endif
