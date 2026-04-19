#include "patternEditor.hpp"
#include "chords.hpp"
#include <FL/fl_draw.H>
#include <algorithm>
#include <climits>
#include <set>

PatternEditor::PatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, Popup& popup)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      noteLabels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      patternGrid(numRows, numCols, rowHeight, colWidth, snap, popup)
{
    rulerOffsetX = scrollbarW + labelsW;

    const int gridH      = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        int total  = self->noteLabels.getTotalTones();
        int maxOff = std::max(0, total - self->patternGrid.numRows);
        self->setRowOffset(maxOff - (int)sb->value());
    }, this);

    hScrollbar = new GridScrollPane(x + scrollbarW + labelsW, y + rulerH + gridH,
                                    visibleGridW, hScrollH, GridScrollPane::HORIZONTAL);
    hScrollbar->linesize(1);
    hScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setColOffset((int)sb->value());
    }, this);
    hScrollbar->hide();

    noteLabels.position(x + scrollbarW, y + rulerH);
    patternGrid.position(x + scrollbarW + labelsW, y + rulerH);
    patternGrid.size(visibleGridW, gridH);
    patternGrid.setPlayhead(&playhead);

    add(*scrollbar);
    add(*hScrollbar);
    add(noteLabels);
    add(patternGrid);

    playhead.setOwner(this);
    seekingEnabled = false;

    noteLabels.onFocus = [this]() { focusPattern(); };

    patternGrid.onDisabledDegreesChanged = [this](const std::vector<int>& dd, int gs, const std::set<int>& occ) {
        int oldTotal = noteLabels.getTotalTones();
        noteLabels.setDisabledDegrees(dd, gs, occ);
        if (noteLabels.getTotalTones() != oldTotal)
            setRowOffset(noteLabels.getRowOffset());
    };

    // initialise horizontal scroll state
    int totalGridW = numCols * colWidth;
    if (totalGridW > visibleGridW) {
        hScrollbar->value(0, visibleGridW / colWidth, 0, numCols);
        hScrollbar->show();
    }

    end();
}

void PatternEditor::setNoteParams(int root, int chord, bool sharp)
{
    int oldChordType = chordType;
    rootPitch = root;
    chordType = chord;
    noteLabels.setParams(root, chord, sharp);
    patternGrid.setChordSize(chordDefs[chord].size);

    int patId = -1;
    if (timeline && lastSelectedTrack >= 0) {
        const auto& tracks = timeline->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].patternId;
    }

    if (timeline && patId > 0 && chordDefs[oldChordType].size != chordDefs[chord].size)
        timeline->remapPatternNotes(patId, chordDefs[oldChordType].size, chordDefs[chord].size);

    setRowOffset(computeDefaultOffset(patId));
}

int PatternEditor::computeDefaultOffset(int patId) const
{
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = 12 + rootSemitone;
    int size         = chordDefs[chordType].size;
    int total        = noteLabels.getTotalTones();

    auto midiForTone = [&](int n) {
        return rootMidi0 + chordDefs[chordType].intervals[n % size] + (n / size) * 12;
    };

    std::vector<Note> allNotes;
    if (timeline && patId >= 0)
        allNotes = timeline->buildPatternNotes(patId);

    int maxOffset = std::max(0, total - patternGrid.numRows);

    int gs = patternGrid.getGroupSize();
    int cs = patternGrid.getChordSize();

    // number of enabled chord tones (for searching by MIDI)
    int numChordTones = (total / gs) * cs;

    if (allNotes.empty()) {
        const int A3 = 57;
        int bestChordTone = 0;
        for (int n = 0; n < numChordTones; n++) {
            if (midiForTone(n) <= A3) bestChordTone = n;
        }
        // Convert chord-tone index to virtual-row index
        int octave     = bestChordTone / cs;
        int degree     = bestChordTone % cs;
        int virtualPos = octave * gs + degree;
        return std::clamp(virtualPos - 1, 0, maxOffset);
    } else {
        // Find lowest enabled note in chord-space, convert to virtual row
        int lowest = INT_MAX;
        for (const auto& n : allNotes)
            if (!n.disabled) lowest = std::min(lowest, (int)n.pitch);
        if (lowest == INT_MAX) lowest = 0;
        int octave     = lowest / cs;
        int degree     = lowest % cs;
        int virtualPos = octave * gs + degree;
        return std::clamp(virtualPos - 1, 0, maxOffset);
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

void PatternEditor::setColOffset(int offset)
{
    if (!hScrollbar) return;
    int visibleCols = patternGrid.w() / patternGrid.colWidth;
    colOffset = std::clamp(offset, 0, std::max(0, patternGrid.numCols - visibleCols));
    hScrollPixel = colOffset * patternGrid.colWidth;
    patternGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, patternGrid.numCols);

    int totalGridW = patternGrid.numCols * patternGrid.colWidth;
    if (totalGridW > patternGrid.w())
        hScrollbar->show();
    else
        hScrollbar->hide();
    redraw();
}

void PatternEditor::resize(int x, int /*y*/, int w, int h)
{
    Fl_Widget::resize(x, y(), w, h);

    int gy           = y();
    int newNumRows   = std::max(1, (h - rulerH - hScrollH) / patternGrid.rowHeight);
    int visibleGridW = std::max(1, w - scrollbarW - labelsW);
    int gridH        = newNumRows * patternGrid.rowHeight;

    scrollbar->resize(x, gy + rulerH, scrollbarW, gridH);

    noteLabels.setNumRows(newNumRows);
    noteLabels.resize(x + scrollbarW, gy + rulerH, labelsW, gridH);

    patternGrid.setNumRows(newNumRows);
    patternGrid.resize(x + scrollbarW + labelsW, gy + rulerH, visibleGridW, gridH);

    hScrollbar->resize(x + scrollbarW + labelsW, gy + rulerH + gridH, visibleGridW, hScrollH);

    // Recompute vertical scroll range with updated numRows
    setRowOffset(noteLabels.getRowOffset());

    // Recompute horizontal scroll
    int totalGridW = patternGrid.numCols * patternGrid.colWidth;
    if (totalGridW > visibleGridW) {
        int visibleCols = visibleGridW / patternGrid.colWidth;
        colOffset    = std::clamp(colOffset, 0, std::max(0, patternGrid.numCols - visibleCols));
        hScrollPixel = colOffset * patternGrid.colWidth;
        patternGrid.setColOffset(colOffset);
        hScrollbar->value(colOffset, visibleCols, 0, patternGrid.numCols);
        hScrollbar->show();
    } else {
        colOffset    = 0;
        hScrollPixel = 0;
        patternGrid.setColOffset(0);
        hScrollbar->hide();
    }

    redraw();
}

int PatternEditor::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        if (Fl::event_dx() != 0)
            setColOffset(colOffset + Fl::event_dx());
        else
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
    const auto& tl  = timeline->get();
    int sel         = tl.selectedTrackIndex;
    bool trackChanged = (sel != lastSelectedTrack);
    lastSelectedTrack = sel;

    if (sel < 0 || sel >= (int)tl.tracks.size()) return;
    int patId = tl.tracks[sel].patternId;

    if (trackChanged && patId > 0) {
        playhead.setPatternTrack(sel);
        patternGrid.setTimeline(timeline, patId);
        setRowOffset(computeDefaultOffset(patId));
        lastLengthBeats = -1.0f;
    }

    float lb = 0.0f;
    for (const auto& p : tl.patterns)
        if (p.id == patId) { lb = p.lengthBeats; break; }

    if (lb != lastLengthBeats && lb > 0.0f) {
        lastLengthBeats      = lb;
        patternGrid.numCols  = (int)lb;
        setColOffset(colOffset);
        redraw();
    }
}
