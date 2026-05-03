#include "patternEditor.hpp"
#include "chords.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <climits>
#include <set>

PatternEditor::PatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, Popup& popup)
    : BasePatternEditor(x, y, visibleW, numRows, numCols, rowHeight, colWidth, snap, labelsW),
      noteLabels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      patternGrid(numRows, numCols, rowHeight, colWidth, snap, popup)
{
    rulerOffsetX = scrollbarW + labelsW;

    const int gridH        = numRows * rowHeight;
    const int paramY       = y + rulerH + gridH;
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

    paramScrollbar = new GridScrollPane(x, paramY, scrollbarW, kParamAreaH);
    paramScrollbar->linesize(1);
    paramScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->paramLaneOffset = (int)sb->value();
        self->paramGrid.setLaneOffset(self->paramLaneOffset);
        self->paramLabels.setLaneOffset(self->paramLaneOffset);
    }, this);
    paramScrollbar->hide();

    hScrollbar = new GridScrollPane(x + scrollbarW + labelsW, paramY,
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

    paramLabels.hide();
    paramGrid.hide();
    paramGrid.setNumCols(numCols);

    add(*scrollbar);
    add(*paramScrollbar);
    add(*hScrollbar);
    add(noteLabels);
    add(patternGrid);
    add(paramLabels);
    add(paramGrid);

    playhead.setOwner(this);
    seekingEnabled = false;

    noteLabels.onFocus = [this]() { focusPattern(); };

    patternGrid.onDisabledDegreesChanged = [this](const std::vector<int>& dd, int gs, const std::set<int>& occ) {
        int oldTotal = noteLabels.getTotalTones();
        noteLabels.setDisabledDegrees(dd, gs, occ);
        if (noteLabels.getTotalTones() != oldTotal)
            setRowOffset(noteLabels.getRowOffset());
    };

    int totalGridW = numCols * colWidth;
    if (totalGridW > visibleGridW) {
        hScrollbar->value(0, visibleGridW / colWidth, 0, numCols);
        hScrollbar->show();
    }

    end();
}

PatternEditor::~PatternEditor() = default;

void PatternEditor::setNoteParams(int root, int chord, bool sharp)
{
    int oldChordType = chordType;
    rootPitch = root;
    chordType = chord;
    noteLabels.setParams(root, chord, sharp);
    patternGrid.setChordSize(chordDefs[chord].size);

    int patId = -1;
    if (pattern && lastSelectedTrack >= 0) {
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].patternId;
    }

    if (pattern && patId > 0 && chordDefs[oldChordType].size != chordDefs[chord].size)
        pattern->remapPatternNotes(patId, chordDefs[oldChordType].size, chordDefs[chord].size);

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
    if (pattern && patId >= 0)
        allNotes = pattern->buildPatternNotes(patId);

    int maxOffset = std::max(0, total - patternGrid.numRows);

    int gs = patternGrid.getGroupSize();
    int cs = patternGrid.getChordSize();

    int numChordTones = (total / gs) * cs;

    if (allNotes.empty()) {
        const int A3 = 57;
        int bestChordTone = 0;
        for (int n = 0; n < numChordTones; n++) {
            if (midiForTone(n) <= A3) bestChordTone = n;
        }
        int octave     = bestChordTone / cs;
        int degree     = bestChordTone % cs;
        int virtualPos = octave * gs + degree;
        return std::clamp(virtualPos - 1, 0, maxOffset);
    } else {
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

void PatternEditor::focusPattern()
{
    int patId = -1;
    if (pattern && lastSelectedTrack >= 0) {
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].patternId;
    }
    setRowOffset(computeDefaultOffset(patId));
}

void PatternEditor::onTimelineChanged()
{
    if (!pattern) return;
    const auto& tl  = pattern->get();
    int sel         = tl.selectedTrackIndex;
    bool trackChanged = (sel != lastSelectedTrack);
    lastSelectedTrack = sel;

    if (sel < 0 || sel >= (int)tl.tracks.size()) return;
    int patId = tl.tracks[sel].patternId;

    if (trackChanged && patId > 0) {
        playhead.setPatternTrack(sel);
        patternGrid.setPattern(pattern, patId);
        setRowOffset(computeDefaultOffset(patId));
        lastLengthBeats = -1.0f;
        paramGrid.setPattern(pattern, patId);
    } else {
        paramGrid.update(pattern, patId);
    }
    paramLabels.setPattern(pattern, patId);
    updateParamScrollbar();

    float lb = 0.0f;
    for (const auto& p : tl.patterns)
        if (p.id == patId) { lb = p.lengthBeats; break; }

    if (lb != lastLengthBeats && lb > 0.0f) {
        lastLengthBeats      = lb;
        patternGrid.numCols  = (int)lb;
        paramGrid.setNumCols((int)lb);
        playhead.setNumCols((int)lb);
        setColOffset(colOffset);
        redraw();
    }
}
