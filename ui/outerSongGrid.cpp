#include "outerSongGrid.hpp"

OuterSongGrid::OuterSongGrid(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, Popup& popup)
    : OuterGrid(x, y, numCols * colWidth, rulerH + numRows * rowHeight, numCols, colWidth),
      songGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
    songGrid.position(x, y + rulerH);
    songGrid.setPlayhead(&playhead);
    add(songGrid);
    playhead.setOwner(this);
    end();
}

void OuterSongGrid::setTransport(ITransport* t, ObservableTimeline* tl)
{
    playhead.setTransport(t, tl);
    playhead.onEndReached = [this]() { if (onEndReached) onEndReached(); };
    songGrid.setTimeline(tl);
}

void OuterSongGrid::setTrackView(int trackIndex, bool beatResolution)
{
    songGrid.setTrackView(trackIndex, beatResolution);
    playhead.setPatternTrack(trackIndex);
}
