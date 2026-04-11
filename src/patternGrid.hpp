#ifndef PATTERN_GRID_HPP
#define PATTERN_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"
#include <vector>
#include <functional>
#include <set>

class PatternGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline        = nullptr;
    int                 patternId       = -1;
    int                 chordSize       = 3;
    int                 rowOffset       = 0;   // in virtual-row units
    std::vector<int>    disabledDegrees;        // sorted ascending; unique disabled degrees
    int                 groupSize       = 3;   // chordSize + disabledDegrees.size()

    void rebuildNotes();

    // Convert a virtual row index to chord-space abs_row (-1 if in a disabled slot)
    int virtualToAbsRow(int virtualPos) const;

protected:
    Fl_Color columnColor(int col) const override;
    Fl_Color rowLineColor(int i)  const override;
    Fl_Color rowBgColor(int row)  const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void toggleNote() override;

public:
    // Fired on every rebuild; args: (disabledDegrees, groupSize, occupiedDisabledVirtualPositions)
    std::function<void(const std::vector<int>&, int, const std::set<int>&)> onDisabledDegreesChanged;

    PatternGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup);
    ~PatternGrid();

    void setTimeline(ObservableTimeline* tl, int patId);
    void setChordSize(int size) { chordSize = size; groupSize = size + (int)disabledDegrees.size(); redraw(); }
    void setNumRows(int n) { numRows = n; rebuildNotes(); }
    void setRowOffset(int offset);
    void onTimelineChanged() override;

    int getGroupSize()       const { return groupSize; }
    int getChordSize()       const { return chordSize; }
    int getRowOffset()       const { return rowOffset; }
};

#endif
