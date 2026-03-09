#include "patternEditor.hpp"

PatternEditor::PatternEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, Popup& popup)
    : Editor(x, y, numCols * colWidth, rulerH + numRows * rowHeight, numCols, colWidth),
      patternGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
    patternGrid.position(x, y + rulerH);
    patternGrid.setPlayhead(&playhead);
    add(patternGrid);
    playhead.setOwner(this);
    seekingEnabled = false;
    end();
}

void PatternEditor::setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex)
{
    playhead.setTransport(t, tl);
    playhead.setPatternTrack(trackIndex);
    patternGrid.setDisplayTimeline(tl);
}
