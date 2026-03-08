#include "patternGrid.hpp"
#include "playhead.hpp"

PatternGrid::PatternGrid(std::vector<Note> notes, int numRows, int numCols,
                         int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{}

Fl_Color PatternGrid::columnColor(int col) const
{
    if (!displayTl) return 0x00EE0000;
    int queryBar = playhead ? (int)playhead->currentBar() : 0;
    int top, bottom;
    displayTl->timeSigAt(queryBar, top, bottom);
    int beatsPerBar = top;
    bool isBarStart = beatsPerBar > 0 && col % beatsPerBar == 0;
    return isBarStart ? 0x00660000 : 0x00EE0000;
}
