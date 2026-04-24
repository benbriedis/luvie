#ifndef PIANOROLL_GRID_HPP
#define PIANOROLL_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"

class PianorollGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline  = nullptr;
    int                 patternId = -1;
    int                 rowOffset = 0;

    void rebuildNotes();

protected:
    Fl_Color columnColor(int col) const override;
    Fl_Color rowLineColor(int i)  const override;
    Fl_Color rowBgColor(int row)  const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void toggleNote() override;

public:
    static constexpr int totalRows = 128;

    PianorollGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup);
    ~PianorollGrid();

    void setTimeline(ObservableTimeline* tl, int patId);
    void setRowOffset(int offset);
    void setNumRows(int n) { numRows = n; rebuildNotes(); }
    void onTimelineChanged() override;

    int getRowOffset() const { return rowOffset; }
};

#endif
