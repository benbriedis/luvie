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
    std::function<void(int,int)> makeTransposeCallback(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void toggleNote() override;

public:
    static constexpr int totalRows    = 128;
    static constexpr int maxSemitones = 12;   // most a transpose offers, either way

    PianorollGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~PianorollGrid();

    void setPattern(ObservablePattern* tl, int patId);
    void setRowOffset(int offset);
    void setNumRows(int n) { numRows = n; rebuildNotes(); }
    void onTimelineChanged() override;

    int getRowOffset() const { return rowOffset; }
};

#endif
