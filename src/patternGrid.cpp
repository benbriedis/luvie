#include "patternGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <set>

PatternGrid::PatternGrid(std::vector<Note> notes, int numRows, int numCols,
                         int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{}

PatternGrid::~PatternGrid()
{
    swapObserver(timeline, nullptr, this);
}

void PatternGrid::setTimeline(ObservableTimeline* tl, int patId)
{
    swapObserver(timeline, tl, this);
    patternId = patId;
    rebuildNotes();
    redraw();
}

// Virtual row layout per octave group (groupSize rows, bottom→top):
//   positions 0..chordSize-1      : valid degrees 0..chordSize-1
//   positions chordSize..groupSize-1: disabled degrees (sorted ascending by disabledDegree)
//
// virtualPos = octave * groupSize + posInGroup
// visual row = rowOffset + numRows - 1 - virtualPos   (top of grid = highest pitch)

int PatternGrid::virtualToAbsRow(int virtualPos) const
{
    if (groupSize <= 0) return virtualPos;
    int gs = groupSize;
    int pos = ((virtualPos % gs) + gs) % gs;
    if (pos >= chordSize) return -1;  // disabled slot
    int octave = virtualPos / gs;
    return octave * chordSize + pos;
}

void PatternGrid::rebuildNotes()
{
    notes.clear();
    if (!timeline || patternId < 0) { clampSelection(); return; }

    auto patNotes = timeline->buildPatternNotes(patternId);

    // Recompute disabled degrees from current pattern
    std::set<int> ddSet;
    for (const auto& n : patNotes)
        if (n.disabled && n.disabledDegree >= 0)
            ddSet.insert(n.disabledDegree);

    std::vector<int> newDD(ddSet.begin(), ddSet.end());  // sorted ascending
    int newGroupSize = chordSize + (int)newDD.size();

    if (newDD != disabledDegrees || newGroupSize != groupSize) {
        disabledDegrees = newDD;
        groupSize       = newGroupSize;
    }

    std::set<int> occupiedDisabledVPos;
    for (auto n : patNotes) {
        int virtualPos;
        if (n.disabled) {
            int octave = n.pitch;
            auto it = std::find(disabledDegrees.begin(), disabledDegrees.end(), n.disabledDegree);
            if (it == disabledDegrees.end()) continue;
            int ddIdx = (int)std::distance(disabledDegrees.begin(), it);
            virtualPos = octave * groupSize + chordSize + ddIdx;
            occupiedDisabledVPos.insert(virtualPos);
        } else {
            int octave = n.pitch / chordSize;
            int degree = n.pitch % chordSize;
            virtualPos = octave * groupSize + degree;
        }
        int visual = (rowOffset + numRows - 1) - virtualPos;
        if (visual >= 0 && visual < numRows) {
            n.pitch = visual;
            notes.push_back(n);
        }
    }

    if (onDisabledDegreesChanged)
        onDisabledDegreesChanged(disabledDegrees, groupSize, occupiedDisabledVPos);

    clampSelection();
}

void PatternGrid::onTimelineChanged()
{
    if (!isActiveDrag())
        rebuildNotes();
    redraw();
}

void PatternGrid::toggleNote()
{
    int   ey        = Fl::event_y() - y();
    int   ex        = Fl::event_x() - x();
    int   gridRight = std::min(w(), (numCols - colOffset) * colWidth);
    if (ex >= gridRight) return;

    int   visual_row = ey / rowHeight;
    float col        = (float)(ex / colWidth) + colOffset;

    if (!timeline || patternId < 0) {
        Grid::toggleNote();
        return;
    }

    // Remove a note if one is at exactly this position
    for (auto& n : notes) {
        if (n.pitch == visual_row && n.beat == col) {
            timeline->removeNote(n.id);
            return;
        }
    }

    // Determine abs_row in chord space; bail if we're in a disabled slot
    int virtualPos = rowOffset + numRows - 1 - visual_row;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.pitch == visual_row
                                  && col < n.beat + n.length
                                  && col + 1.0f > n.beat; });
    if (clear)
        timeline->addNote(patternId, col, (float)abs_row, 1.0f);
}

std::function<void()> PatternGrid::makeDeleteCallback(int noteIdx)
{
    if (!timeline) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { timeline->removeNote(id); };
}

void PatternGrid::onCommitMove(const StateDragMove& s)
{
    if (!timeline || notes[s.noteIdx].disabled) return;
    int id         = notes[s.noteIdx].id;
    int virtualPos = rowOffset + numRows - 1 - (int)notes[s.noteIdx].pitch;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;
    timeline->moveNote(id, notes[s.noteIdx].beat, (float)abs_row);
}

void PatternGrid::onCommitResize(const StateDragResize& s)
{
    if (!timeline || notes[s.noteIdx].disabled) return;
    int id = notes[s.noteIdx].id;
    if (s.side == LEFT)
        timeline->resizeNoteLeft(id, notes[s.noteIdx].beat, notes[s.noteIdx].length);
    else
        timeline->resizeNoteRight(id, notes[s.noteIdx].length);
}

void PatternGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    rebuildNotes();
    redraw();
}

// Row line i sits between visual rows i-1 and i.
// The corresponding virtual position below line i is: rowOffset + numRows - i
// Dark if that position is the bottom of an octave group (virtualPos % groupSize == 0).
Fl_Color PatternGrid::rowLineColor(int i) const
{
    if (i <= 0 || i >= numRows || groupSize <= 0) return 0xEE888800;
    if ((rowOffset + numRows - i) % groupSize == 0)
        return 0x33110000;  // dark octave boundary
    return 0xEE888800;
}

// Grey background for disabled slots within each octave group.
Fl_Color PatternGrid::rowBgColor(int row) const
{
    for (const auto& n : notes)
        if (n.disabled && (int)n.pitch == row) return 0xCCCCCC00;
    return bgColor;
}

Fl_Color PatternGrid::columnColor(int col) const
{
    if (!timeline) return 0x00EE0000;
    int queryBar = playhead ? (int)playhead->currentBar() : 0;
    int top, bottom;
    timeline->timeSigAt(queryBar, top, bottom);
    int beatsPerBar = top;
    bool isBarStart = beatsPerBar > 0 && col % beatsPerBar == 0;
    return isBarStart ? 0x00660000 : 0x00EE0000;
}
