#ifndef PATTERN_GRID_HPP
#define PATTERN_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"

class PatternGrid : public Grid {
    const ObservableTimeline* displayTl = nullptr;

protected:
    Fl_Color columnColor(int col) const override;

public:
    PatternGrid(std::vector<Note> notes, int numRows, int numCols,
                int rowHeight, int colWidth, float snap, Popup& popup);

    void setDisplayTimeline(const ObservableTimeline* tl) { displayTl = tl; }
};

#endif
