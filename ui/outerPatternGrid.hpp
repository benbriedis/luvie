#ifndef OUTER_PATTERN_GRID_HPP
#define OUTER_PATTERN_GRID_HPP

#include "outerGrid.hpp"
#include "patternGrid.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <vector>

class OuterPatternGrid : public OuterGrid {
    PatternGrid patternGrid;

public:
    OuterPatternGrid(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                     int rowHeight, int colWidth, float snap, Popup& popup);

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setTimeline(ObservableTimeline* tl, int patternId) { patternGrid.setTimeline(tl, patternId); }
    int  numPatternBeats() const { return patternGrid.numCols; }
};

#endif
