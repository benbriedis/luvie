#ifndef PATTERN_EDITOR_HPP
#define PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "patternGrid.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <vector>

class PatternEditor : public Editor {
    PatternGrid patternGrid;

public:
    PatternEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, Popup& popup);

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setTimeline(ObservableTimeline* tl, int patternId) { patternGrid.setTimeline(tl, patternId); }
    int  numPatternBeats() const { return patternGrid.numCols; }
};

#endif
