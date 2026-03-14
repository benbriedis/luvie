#ifndef PATTERN_GRID_HPP
#define PATTERN_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"

class PatternGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline   = nullptr;
    int                       patternId  = -1;
    int                       draggingNoteId = -1;
    bool                      isDragging = false;

    void rebuildNotes();

protected:
    Fl_Color columnColor(int col) const override;
    std::function<void()> makeDeleteCallback() override;
    void onBeginDrag() override;
    void onCommitDrag() override;
    void toggleNote() override;

public:
    PatternGrid(std::vector<Note> notes, int numRows, int numCols,
                int rowHeight, int colWidth, float snap, Popup& popup);
    ~PatternGrid();

    void setTimeline(ObservableTimeline* tl, int patId);
    void onTimelineChanged() override;
};

#endif
