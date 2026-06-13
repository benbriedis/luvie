#ifndef PATTERN_GRID_HPP
#define PATTERN_GRID_HPP

#include "grid.hpp"
#include "observablePattern.hpp"
#include <vector>
#include <functional>
#include <set>
#include <optional>

class PatternGrid : public Grid, public ITimelineObserver {
    ObservablePattern* pattern        = nullptr;
    int                 patternId       = -1;
    int                 chordSize       = 3;
    int                 rowOffset       = 0;   // in virtual-row units
    std::vector<int>    disabledDegrees;        // sorted ascending; unique disabled degrees
    int                 groupSize       = 3;   // chordSize + disabledDegrees.size()

    struct RapidCell {
        int row, col;
        bool operator==(const RapidCell& o) const { return row == o.row && col == o.col; }
    };

    bool                         rapidMode           = false;
    bool                         rapidRemovedOnClick = false;
    std::set<std::pair<int,int>> rapidCells;
    std::optional<RapidCell>     rapidLast;
    std::optional<RapidCell>     rapidPending;

    void rebuildNotes();
    bool screenToCell(int ex, int ey, int& outRow, int& outAbsCol) const;
    void rapidTryCreate(int visualRow, int absCol);
    void processRapidCell(RapidCell cur);
    static bool rapidIsDiagonal(RapidCell a, RapidCell b) {
        return std::abs(a.row - b.row) == 1 && std::abs(a.col - b.col) == 1;
    }

    // Convert a virtual row index to chord-space abs_row (-1 if in a disabled slot)
    int virtualToAbsRow(int virtualPos) const;

protected:
    Fl_Color columnColor(int col) const override;
    Fl_Color rowLineColor(int i)  const override;
    Fl_Color rowBgColor(int row)  const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    std::function<void(float)> makeVelocityCallback(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void toggleNote() override;

    int handle(int event) override;

public:
    // Fired on every rebuild; args: (disabledDegrees, groupSize, occupiedDisabledVirtualPositions)
    std::function<void(const std::vector<int>&, int, const std::set<int>&)> onDisabledDegreesChanged;

    PatternGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~PatternGrid();

    void setPattern(ObservablePattern* tl, int patId);
    void setChordSize(int size) { chordSize = size; groupSize = size + (int)disabledDegrees.size(); redraw(); }
    void setNumRows(int n) { numRows = n; rebuildNotes(); }
    void setRowOffset(int offset);
    void setRapidMode(bool r);
    void onTimelineChanged() override;

    int getGroupSize()       const { return groupSize; }
    int getChordSize()       const { return chordSize; }
    int getRowOffset()       const { return rowOffset; }
};

#endif
