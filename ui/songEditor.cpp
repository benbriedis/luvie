#include "songEditor.hpp"

SongEditor::SongEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                       int rowHeight, int colWidth, float snap, Popup& popup)
    : Editor(x, y, labelW + numCols * colWidth, rulerH + numRows * rowHeight, numCols, colWidth),
      trackLabels(x, y + rulerH, labelW, rowHeight),
      songGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
    rulerOffsetX = labelW;
    trackLabels.position(x, y + rulerH);
    songGrid.position(x + labelW, y + rulerH);
    songGrid.setPlayhead(&playhead);
    add(trackLabels);
    add(songGrid);
    playhead.setOwner(this);
    end();
}

void SongEditor::setTransport(ITransport* t, ObservableTimeline* tl)
{
    playhead.setTransport(t, tl);
    playhead.onEndReached = [this]() { if (onEndReached) onEndReached(); };
    songGrid.setTimeline(tl);
    trackLabels.setTimeline(tl);
}

void SongEditor::setTrackView(int trackIndex, bool beatResolution)
{
    songGrid.setTrackView(trackIndex, beatResolution);
    playhead.setPatternTrack(trackIndex);
}
