#include "outerPatternGrid.hpp"

OuterPatternGrid::OuterPatternGrid(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                                   int rowHeight, int colWidth, float snap, Popup& popup)
    : OuterGrid(x, y, numCols * colWidth, rulerH + numRows * rowHeight, numCols, colWidth),
      patternGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
    patternGrid.position(x, y + rulerH);
    patternGrid.setPlayhead(&playhead);
    add(patternGrid);
    playhead.setOwner(this);
    seekingEnabled = false;
    end();
}

void OuterPatternGrid::setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex)
{
    playhead.setTransport(t, tl);
    playhead.setPatternTrack(trackIndex);
    patternGrid.setDisplayTimeline(tl);
}
