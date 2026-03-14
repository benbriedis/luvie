#include "patternEditor.hpp"

PatternEditor::PatternEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, Popup& popup)
    : Editor(x, y, labelsW + numCols * colWidth, rulerH + numRows * rowHeight, numCols, colWidth),
      noteLabels(x, y + rulerH, labelsW, numRows, rowHeight),
      patternGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
    rulerOffsetX = labelsW;
    noteLabels.position(x, y + rulerH);
    patternGrid.position(x + labelsW, y + rulerH);
    patternGrid.setPlayhead(&playhead);
    add(noteLabels);
    add(patternGrid);
    playhead.setOwner(this);
    seekingEnabled = false;
    end();
}

void PatternEditor::setNoteParams(int octave, int rootPitch, int chordType, bool useSharp)
{
    static const int chordSizes[] = {3, 3, 4, 4};
    noteLabels.setParams(octave, rootPitch, chordType, useSharp);
    patternGrid.setChordSize(chordSizes[chordType]);
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
