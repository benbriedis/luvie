#include "patternGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <set>

PatternGrid::PatternGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
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
            int octave = n.row;
            auto it = std::find(disabledDegrees.begin(), disabledDegrees.end(), n.disabledDegree);
            if (it == disabledDegrees.end()) continue;
            int ddIdx = (int)std::distance(disabledDegrees.begin(), it);
            virtualPos = octave * groupSize + chordSize + ddIdx;
            occupiedDisabledVPos.insert(virtualPos);
        } else {
            int octave = n.row / chordSize;
            int degree = n.row % chordSize;
            virtualPos = octave * groupSize + degree;
        }
        int visual = (rowOffset + numRows - 1) - virtualPos;
        if (visual >= 0 && visual < numRows) {
            n.row = visual;
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
        if (n.row == visual_row && n.beat == col) {
            pattern->removeNote(n.id);
            return;
        }
    }

    // Determine abs_row in chord space; bail if we're in a disabled slot
    int virtualPos = rowOffset + numRows - 1 - visual_row;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == visual_row
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

std::function<void(float)> PatternGrid::makeVelocityCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id](float v) { pattern->setNoteVelocity(id, v); };
}

void PatternGrid::onCommitMove(const StateDragMove& s)
{
    if (!pattern || notes[s.noteIdx].disabled) return;
    int id         = notes[s.noteIdx].id;
    int virtualPos = rowOffset + numRows - 1 - (int)notes[s.noteIdx].row;
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
    rapidMode           = r;
    rapidRemovedOnClick = false;
    rapidCells.clear();
    rapidLast    = std::nullopt;
    rapidPending = std::nullopt;
    state = StateIdle{};
    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
    redraw();
}

bool PatternGrid::screenToCell(int ex, int ey, int& outRow, int& outAbsCol) const
{
    int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
    if (ex < 0 || ex >= gridRight || ey < 0 || ey >= h()) return false;
    outRow    = ey / rowHeight;
    outAbsCol = ex / colWidth + colOffset;
    return true;
}

void PatternGrid::rapidTryCreate(int visualRow, int absCol)
{
    if (visualRow < 0 || visualRow >= numRows || absCol < 0 || absCol + 1 > numCols) return;

    auto key = std::make_pair(visualRow, absCol);
    if (rapidCells.count(key)) return;
    rapidCells.insert(key);

    float col = float(absCol);
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) {
            return (int)n.row == visualRow
                && col < n.beat + n.length
                && col + 1.0f > n.beat;
        });
    if (!clear || !pattern || patternId < 0) return;

    int virtualPos = rowOffset + numRows - 1 - visualRow;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;

    pattern->addNote(patternId, col, abs_row, 1.0f);
}

void PatternGrid::processRapidCell(RapidCell cur)
{
    if (!rapidPending) {
        if (!rapidLast || rapidIsDiagonal(*rapidLast, cur)) {
            rapidTryCreate(cur.row, cur.col);
            rapidLast = cur;
        } else {
            rapidPending = cur;
        }
    } else {
        if (rapidLast && rapidIsDiagonal(*rapidLast, cur)) {
            // cur is diagonal to last placed — the pending cell was a stepping stone, skip it
            rapidTryCreate(cur.row, cur.col);
            rapidLast    = cur;
            rapidPending = std::nullopt;
        } else {
            // not diagonal — commit the pending cell, then reconsider cur from there
            RapidCell pending = *rapidPending;
            rapidPending = std::nullopt;
            rapidTryCreate(pending.row, pending.col);
            rapidLast = pending;
            processRapidCell(cur);
        }
    }
}

int PatternGrid::handle(int event)
{
    if (!rapidMode)
        return Grid::handle(event);

    switch (event) {
    case FL_PUSH: {
        rapidCells.clear();
        rapidLast    = std::nullopt;
        rapidPending = std::nullopt;

        if (Fl::event_button() == FL_LEFT_MOUSE) {
            int row, absCol;
            if (screenToCell(Fl::event_x() - x(), Fl::event_y() - y(), row, absCol)) {
                float col = float(absCol);
                bool removed = false;
                if (pattern && patternId >= 0) {
                    for (const auto& n : notes) {
                        if ((int)n.row == row && n.beat == col) {
                            pattern->removeNote(n.id);
                            removed = true;
                            break;
                        }
                    }
                }
                        rapidLast           = RapidCell{row, absCol};
                rapidRemovedOnClick = removed;
                if (!removed)
                    rapidTryCreate(row, absCol);
            }
        }
        return 1;
    }
    case FL_DRAG: {
        if (!Fl::event_state(FL_BUTTON1)) return 1;
        int row, absCol;
        if (!screenToCell(Fl::event_x() - x(), Fl::event_y() - y(), row, absCol)) return 1;
        RapidCell cur{row, absCol};
        if ((rapidLast    && cur == *rapidLast)    ||
            (rapidPending && cur == *rapidPending)) return 1;
        if (rapidRemovedOnClick && rapidLast) {
            rapidTryCreate(rapidLast->row, rapidLast->col);
            rapidRemovedOnClick = false;
        }
        processRapidCell(cur);
        return 1;
    }
    case FL_RELEASE:
        if (rapidPending) {
            rapidTryCreate(rapidPending->row, rapidPending->col);
            rapidPending = std::nullopt;
        }
        rapidRemovedOnClick = false;
        rapidCells.clear();
        rapidLast = std::nullopt;
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
        if (n.disabled && (int)n.row == row) return 0xCCCCCC00;
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
