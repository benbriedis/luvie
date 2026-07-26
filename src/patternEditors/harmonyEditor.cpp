#include "harmonyEditor.hpp"
#include "chords.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <climits>
#include <set>

HarmonyEditor::HarmonyEditor(int x, int y, int visibleW, int numRows, int numCols,
                             int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
    : BasePatternEditor(x, y, visibleW, numRows, numCols, rowHeight, colWidth, snap, labelsW),
      harmonyLabels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      harmonyGrid(numRows, numCols, rowHeight, colWidth, snap, popup)
{
    const int gridH        = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    harmonyLabels.position(x + scrollbarW, y + rulerH);
    harmonyGrid.position(x + scrollbarW + labelsW, y + rulerH);
    harmonyGrid.size(visibleGridW, gridH);
    harmonyGrid.setPlayhead(&playhead);

    gridPane.add(harmonyLabels);
    gridPane.add(harmonyGrid);
    gridPane.add(paramLabels);
    gridPane.add(paramGrid);

    playhead.setOwner(this);

    harmonyGrid.onBonusDegreesChanged = [this](const std::vector<int>& dd, int gs) {
        int oldTotal = harmonyLabels.getTotalTones();
        harmonyLabels.setBonusDegrees(dd, gs);
        harmonyGrid.setTotalTones(harmonyLabels.getTotalTones());
        if (harmonyLabels.getTotalTones() != oldTotal)
            setRowOffset(harmonyLabels.getRowOffset());
    };

    end();
}

HarmonyEditor::~HarmonyEditor() = default;

// Display-only sync of the editor to a pattern's harmony. Never remaps notes:
// remapping on a chord-size change is an interactive edit handled in
// PatternPanel::commitHarmony, so that merely *switching* to a pattern with a
// different-size chord does not corrupt that pattern's stored notes.
void HarmonyEditor::setNoteParams(int root, std::string_view chordHash, bool sharp)
{
    int  newChordIndex = chordIndexForHash(chordHash);
    bool paramsChanged = (root != rootPitch) || (newChordIndex != chordIndex);

    rootPitch  = root;
    chordIndex = newChordIndex;
    harmonyLabels.setParams(root, chordHash, sharp);
    harmonyGrid.setChordSize(chordDefs[chordIndex].size);
    harmonyGrid.setTotalTones(harmonyLabels.getTotalTones());

    // This runs on every timeline change (via PatternPanel::onParamsChanged), so
    // only refocus the view when the harmony genuinely changed — otherwise adding
    // a note would recompute the default offset and scroll the grid. Pattern
    // switches refocus independently through setGridPattern.
    if (!paramsChanged) return;

    int patId = -1;
    if (pattern && lastSelectedTrack >= 0) {
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].lanes.empty() ? 0 : tracks[lastSelectedTrack].lanes[0].patternId;
    }

    setRowOffset(computeDefaultOffset(patId));
}

int HarmonyEditor::computeDefaultOffset(int patId) const
{
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = 12 + rootSemitone;
    int total        = harmonyLabels.getTotalTones();

    auto midiForTone = [&](int n) {
        return rootMidi0 + chordToneOffset(chordDefs[chordIndex], n);
    };

    std::vector<Note> allNotes;
    if (pattern && patId >= 0)
        allNotes = pattern->buildPatternNotes(patId);

    int maxOffset = std::max(0, total - harmonyGrid.numRows);

    int gs = harmonyGrid.getPitchGroupSize();
    int cs = harmonyGrid.getChordSize();

    int numChordTones = (total / gs) * cs;

    if (allNotes.empty()) {
        const int A3 = 57;
        int bestChordTone = 0;
        for (int n = 0; n < numChordTones; n++) {
            if (midiForTone(n) <= A3) bestChordTone = n;
        }
        int pitchGroup = bestChordTone / cs;
        int degree     = bestChordTone % cs;
        int virtualPos = pitchGroup * gs + degree;
        return std::clamp(virtualPos - 1, 0, maxOffset);
    } else {
        int lowest = INT_MAX;
        for (const auto& n : allNotes)
            if (!n.bonus) lowest = std::min(lowest, (int)n.row);
        if (lowest == INT_MAX) lowest = 0;
        int pitchGroup = lowest / cs;
        int degree     = lowest % cs;
        int virtualPos = pitchGroup * gs + degree;
        return std::clamp(virtualPos - 1, 0, maxOffset);
    }
}

void HarmonyEditor::focusPattern()
{
    int patId = -1;
    if (pattern && lastSelectedTrack >= 0) {
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack < (int)tracks.size())
            patId = tracks[lastSelectedTrack].lanes.empty() ? 0 : tracks[lastSelectedTrack].lanes[0].patternId;
    }
    setRowOffset(computeDefaultOffset(patId));
}

void HarmonyEditor::setGridPattern(int patId)
{
    if (patId <= 0) return;
    harmonyGrid.setPattern(pattern, patId);
    setRowOffset(computeDefaultOffset(patId));
}
