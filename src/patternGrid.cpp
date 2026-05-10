#include "patternGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <set>

PatternGrid::PatternGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(numRows, numCols, rowHeight, colWidth, snap, popup)
{}

PatternGrid::~PatternGrid()
{
    swapObserver(pattern, nullptr, this);
}

void PatternGrid::setPattern(ObservablePattern* tl, int patId)
{
    swapObserver(pattern, tl, this);
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
    if (!pattern || patternId < 0) { clampSelection(); return; }

    auto patNotes = pattern->buildPatternNotes(patternId);

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

    if (!pattern || patternId < 0) {
        Grid::toggleNote();
        return;
    }

    // Remove a note if one is at exactly this position
    for (auto& n : notes) {
        if (n.pitch == visual_row && n.beat == col) {
            pattern->removeNote(n.id);
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
        pattern->addNote(patternId, col, abs_row, 1.0f);
}

std::function<void()> PatternGrid::makeDeleteCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { pattern->removeNote(id); };
}

void PatternGrid::onCommitMove(const StateDragMove& s)
{
    if (!pattern || notes[s.noteIdx].disabled) return;
    int id         = notes[s.noteIdx].id;
    int virtualPos = rowOffset + numRows - 1 - (int)notes[s.noteIdx].pitch;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;
    pattern->moveNote(id, notes[s.noteIdx].beat, (float)abs_row);
}

void PatternGrid::onCommitResize(const StateDragResize& s)
{
    if (!pattern || notes[s.noteIdx].disabled) return;
    int id = notes[s.noteIdx].id;
    if (s.side == Side::Left)
        pattern->resizeNoteLeft(id, notes[s.noteIdx].beat, notes[s.noteIdx].length);
    else
        pattern->resizeNoteRight(id, notes[s.noteIdx].length);
}

void PatternGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    rebuildNotes();
    redraw();
}

void PatternGrid::setRapidMode(bool r)
{
    rapidMode = r;
    rapidCells.clear();
    state = StateIdle{};
    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
    redraw();
}

void PatternGrid::rapidTryCreate(int ex, int ey)
{
    int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
    if (ex < 0 || ex >= gridRight || ey < 0 || ey >= h()) return;

    int   visual_row = ey / rowHeight;
    int   col_int    = ex / colWidth;
    float col        = float(col_int) + colOffset;

    if (visual_row < 0 || visual_row >= numRows) return;
    if (col + 1.0f > numCols) return;

    auto key = std::make_pair(visual_row, col_int + (int)colOffset);
    if (rapidCells.count(key)) return;
    rapidCells.insert(key);

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) {
            return (int)n.pitch == visual_row
                && col < n.beat + n.length
                && col + 1.0f > n.beat;
        });
    if (!clear || !pattern || patternId < 0) return;

    int virtualPos = rowOffset + numRows - 1 - visual_row;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;

    pattern->addNote(patternId, col, abs_row, 1.0f);
}

int PatternGrid::handle(int event)
{
    if (!rapidMode)
        return Grid::handle(event);

    switch (event) {
    case FL_PUSH: {
        rapidCells.clear();
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            int   ex         = Fl::event_x() - x();
            int   ey         = Fl::event_y() - y();
            int   gridRight  = std::min(w(), (numCols - colOffset) * colWidth);
            if (ex >= 0 && ex < gridRight && ey >= 0 && ey < h()) {
                int   visual_row = ey / rowHeight;
                int   col_int    = ex / colWidth;
                float col        = float(col_int) + colOffset;
                auto  key        = std::make_pair(visual_row, col_int + (int)colOffset);
                bool  removed    = false;
                if (pattern && patternId >= 0) {
                    for (const auto& n : notes) {
                        if ((int)n.pitch == visual_row && n.beat == col) {
                            pattern->removeNote(n.id);
                            removed = true;
                            break;
                        }
                    }
                }
                if (!removed)
                    rapidTryCreate(ex, ey);
            }
        }
        return 1;
    }
    case FL_DRAG:
        if (Fl::event_state(FL_BUTTON1))
            rapidTryCreate(Fl::event_x() - x(), Fl::event_y() - y());
        return 1;
    case FL_RELEASE:
        rapidCells.clear();
        return 1;
    case FL_ENTER:
        window()->cursor(FL_CURSOR_CROSS);
        return 1;
    case FL_MOVE:
        window()->cursor(FL_CURSOR_CROSS);
        return 0;
    case FL_LEAVE:
        window()->cursor(FL_CURSOR_DEFAULT);
        return 0;
    default:
        return 0;
    }
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
    if (!pattern) return 0x00EE0000;
    for (const auto& p : pattern->get().patterns) {
        if (p.id == patternId) {
            bool isBarStart = p.timeSigTop > 0 && col % p.timeSigTop == 0;
            return isBarStart ? 0x00660000 : 0x00EE0000;
        }
    }
    return 0x00EE0000;
}
