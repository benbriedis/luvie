#include "patternEditor.hpp"
#include <FL/fl_draw.H>
#include <algorithm>

static const int intervals[][4] = {
    {0, 4, 7,  0},
    {0, 3, 7,  0},
    {0, 4, 7, 11},
    {0, 3, 7, 10},
};
static const int chordSizes[] = {3, 3, 4, 4};

PatternEditor::PatternEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, Popup& popup)
    : Editor(x, y, scrollbarW + labelsW + numCols * colWidth, rulerH + numRows * rowHeight, numCols, colWidth),
      noteLabels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      patternGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
    rulerOffsetX = scrollbarW + labelsW;

    const int gridH = numRows * rowHeight;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        int total  = self->noteLabels.getTotalTones();
        int maxOff = std::max(0, total - self->patternGrid.numRows);
        self->setRowOffset(maxOff - (int)sb->value());
    }, this);

    noteLabels.position(x + scrollbarW, y + rulerH);
    patternGrid.position(x + scrollbarW + labelsW, y + rulerH);
    patternGrid.setPlayhead(&playhead);

    add(*scrollbar);
    add(noteLabels);
    add(patternGrid);

    playhead.setOwner(this);
    seekingEnabled = false;

    noteLabels.onFocus = [this]() { focusPattern(); };

    end();
}

void PatternEditor::setNoteParams(int root, int chord, bool sharp)
{
    rootPitch = root;
    chordType = chord;
    noteLabels.setParams(root, chord, sharp);
    patternGrid.setChordSize(chordSizes[chord]);

    int patId = -1;
    if (timeline && lastSelectedTrack >= 0) {
        const auto& tracks = timeline->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].patternId;
    }
    setRowOffset(computeDefaultOffset(patId));
}

int PatternEditor::computeDefaultOffset(int patId) const
{
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = 12 + rootSemitone;
    int size         = chordSizes[chordType];
    int total        = noteLabels.getTotalTones();

    auto midiForTone = [&](int n) {
        return rootMidi0 + intervals[chordType][n % size] + (n / size) * 12;
    };

    std::vector<Note> allNotes;
    if (timeline && patId >= 0)
        allNotes = timeline->buildPatternNotes(patId);

    int maxOffset = std::max(0, total - patternGrid.numRows);

    if (allNotes.empty()) {
        const int A3 = 57;
        int best = 0;
        for (int n = 0; n < total; n++) {
            if (midiForTone(n) <= A3) best = n;
        }
        return std::clamp(best - 1, 0, maxOffset);
    } else {
        int lowest = (int)allNotes[0].pitch;
        for (const auto& n : allNotes)
            lowest = std::min(lowest, (int)n.pitch);
        return std::clamp(lowest - 1, 0, maxOffset);
    }
}

void PatternEditor::setRowOffset(int offset)
{
    int total  = noteLabels.getTotalTones();
    int maxOff = std::max(0, total - patternGrid.numRows);
    offset = std::clamp(offset, 0, maxOff);
    noteLabels.setRowOffset(offset);
    patternGrid.setRowOffset(offset);
    if (scrollbar)
        scrollbar->value(maxOff - offset, patternGrid.numRows, 0, total);
}

int PatternEditor::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        setRowOffset(noteLabels.getRowOffset() - Fl::event_dy());
        return 1;
    }
    return Editor::handle(event);
}

void PatternEditor::focusPattern()
{
    int patId = -1;
    if (timeline && lastSelectedTrack >= 0) {
        const auto& tracks = timeline->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].patternId;
    }
    setRowOffset(computeDefaultOffset(patId));
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
        if (patId > 0) {
            patternGrid.setTimeline(timeline, patId);
            setRowOffset(computeDefaultOffset(patId));
        }
    }
}
