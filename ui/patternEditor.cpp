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

PatternEditor::~PatternEditor()
{
    swapObserver(timeline, nullptr, this);
}

void PatternEditor::setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex)
{
    swapObserver(timeline, tl, this);
    playhead.setTransport(t, tl);
    playhead.setPatternTrack(trackIndex);
}

void PatternEditor::onTimelineChanged()
{
    if (!timeline) return;
    int sel = timeline->get().selectedTrackIndex;
    if (sel == lastSelectedTrack) return;
    lastSelectedTrack = sel;
    const auto& tracks = timeline->get().tracks;
    if (sel >= 0 && sel < (int)tracks.size()) {
        playhead.setPatternTrack(sel);
        int patId = tracks[sel].patternId;
        if (patId > 0)
            patternGrid.setTimeline(timeline, patId);
    }
}
