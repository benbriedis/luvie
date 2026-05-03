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
    const int gridH        = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    noteLabels.position(x + scrollbarW, y + rulerH);
    patternGrid.position(x + scrollbarW + labelsW, y + rulerH);
    patternGrid.size(visibleGridW, gridH);
    patternGrid.setPlayhead(&playhead);

    add(noteLabels);
    add(patternGrid);
    add(paramLabels);
    add(paramGrid);

    playhead.setOwner(this);

    noteLabels.onFocus = [this]() { focusPattern(); };

    patternGrid.onDisabledDegreesChanged = [this](const std::vector<int>& dd, int gs, const std::set<int>& occ) {
        int oldTotal = noteLabels.getTotalTones();
        noteLabels.setDisabledDegrees(dd, gs, occ);
        if (noteLabels.getTotalTones() != oldTotal)
            setRowOffset(noteLabels.getRowOffset());
    };

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

void PatternEditor::setGridPattern(int patId)
{
    if (patId <= 0) return;
    patternGrid.setPattern(pattern, patId);
    setRowOffset(computeDefaultOffset(patId));
    lastLengthBeats = -1.0f;
}

void PatternEditor::afterTimelineChanged(int patId)
{
    if (patId <= 0 || !pattern) return;
    float lb = 0.0f;
    for (const auto& p : pattern->get().patterns)
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
