#include "patternGrid.hpp"
#include "transposePopup.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <cmath>
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

// Virtual row layout per pitch group (pitchGroupSize rows, bottom→top):
//   positions 0..chordSize-1           : valid degrees 0..chordSize-1
//   positions chordSize..pitchGroupSize-1: disabled degrees (sorted ascending by disabledDegree)
//
// virtualPos = pitchGroup * pitchGroupSize + posInGroup
// visual row = rowOffset + numRows - 1 - virtualPos   (top of grid = highest pitch)

int PatternGrid::virtualToAbsRow(int virtualPos) const
{
    if (pitchGroupSize <= 0) return virtualPos;
    int gs = pitchGroupSize;
    int pos = ((virtualPos % gs) + gs) % gs;
    if (pos >= chordSize) return -1;  // disabled slot
    int pitchGroup = virtualPos / gs;
    return pitchGroup * chordSize + pos;
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
    int newPitchGroupSize = chordSize + (int)newDD.size();

    if (newDD != disabledDegrees || newPitchGroupSize != pitchGroupSize) {
        disabledDegrees = newDD;
        pitchGroupSize  = newPitchGroupSize;
    }

    for (auto n : patNotes) {
        int virtualPos;
        if (n.disabled) {
            int pitchGroup = n.row;
            auto it = std::find(disabledDegrees.begin(), disabledDegrees.end(), n.disabledDegree);
            if (it == disabledDegrees.end()) continue;
            int ddIdx = (int)std::distance(disabledDegrees.begin(), it);
            virtualPos = pitchGroup * pitchGroupSize + chordSize + ddIdx;
        } else {
            int pitchGroup = n.row / chordSize;
            int degree     = n.row % chordSize;
            virtualPos = pitchGroup * pitchGroupSize + degree;
        }
        int visual = (rowOffset + numRows - 1) - virtualPos;
        if (visual >= 0 && visual < numRows) {
            n.row = visual;
            notes.push_back(n);
        }
    }

    if (onDisabledDegreesChanged)
        onDisabledDegreesChanged(disabledDegrees, pitchGroupSize);

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
    float fcol       = (float)ex / colWidth + colOffset;

    if (!pattern || patternId < 0) {
        Grid::toggleNote();
        return;
    }

    // Clicking inside a note removes it; clicking the empty space a shorter
    // note leaves behind creates a new one.
    for (auto& n : notes) {
        if (hitsNote(n, visual_row, fcol)) {
            pattern->removeNote(n.id);
            return;
        }
    }

    // Determine abs_row in chord space; bail if we're in a disabled slot
    int virtualPos = rowOffset + numRows - 1 - visual_row;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;

    float col    = newNoteStart(fcol);
    float length = newNoteLength();

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == visual_row
                                  && beatsOverlap(col, length, n.beat, n.length); });
    if (clear)
        pattern->addNote(patternId, col, abs_row, length);
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

// Virtual row a stored note occupies — the inverse of the mapping rebuildNotes
// uses. -1 when a disabled note's degree is no longer in the layout.
int PatternGrid::virtualPosOf(const Note& n) const
{
    if (pitchGroupSize <= 0 || chordSize <= 0) return -1;
    if (!n.disabled)
        return (n.row / chordSize) * pitchGroupSize + (n.row % chordSize);

    auto it = std::find(disabledDegrees.begin(), disabledDegrees.end(), n.disabledDegree);
    if (it == disabledDegrees.end()) return -1;
    return n.row * pitchGroupSize + chordSize + (int)std::distance(disabledDegrees.begin(), it);
}

// A row's note-slot: a chord degree in the lower part of each pitch group, one
// of the greyed-out disabled degrees above it. A transposed note takes on the
// character of the row it lands on, which is what "move everything up N rows"
// means visually.
ObservablePattern::NoteRowSlot PatternGrid::slotForVirtualPos(int noteId, int virtualPos) const
{
    int pitchGroup = virtualPos / pitchGroupSize;
    int pos        = virtualPos % pitchGroupSize;
    if (pos < chordSize)
        return {noteId, pitchGroup * chordSize + pos, false, -1};
    return {noteId, pitchGroup, true, disabledDegrees[pos - chordSize]};
}

// Lowest/highest virtual row the pattern's notes occupy; {-1,-1} when it has
// none. Taken from the pattern, not `notes`, which holds only the visible rows.
std::pair<int,int> PatternGrid::virtualPosExtent() const
{
    int lo = -1, hi = -1;
    for (const auto& n : pattern->buildPatternNotes(patternId)) {
        int vp = virtualPosOf(n);
        if (vp < 0) continue;
        if (lo < 0 || vp < lo) lo = vp;
        if (vp > hi) hi = vp;
    }
    return {lo, hi};
}

void PatternGrid::transposeRows(int rows)
{
    if (!pattern || patternId < 0 || rows == 0 || pitchGroupSize <= 0) return;

    std::vector<ObservablePattern::NoteRowSlot> slots;
    for (const auto& n : pattern->buildPatternNotes(patternId)) {
        int vp = virtualPosOf(n);
        if (vp < 0) continue;
        vp += rows;
        if (vp < 0 || vp >= totalTones) return;  // the offered range rules this out
        slots.push_back(slotForVirtualPos(n.id, vp));
    }
    pattern->setNoteRows(patternId, slots);
}

// Transpose applies to the whole pattern, not the clicked note, and counts GUI
// rows: one pitch group is pitchGroupSize rows, which is the most we offer either
// way. Disabled notes move with the rest.
std::function<void(int,int)> PatternGrid::makeTransposeCallback(int noteIdx)
{
    if (!pattern || !transposePopup || patternId < 0 || pitchGroupSize <= 0 || totalTones <= 0)
        return nullptr;
    (void)noteIdx;
    return [this](int px, int py) {
        auto [lo, hi] = virtualPosExtent();
        if (lo < 0) return;
        transposePopup->open(px, py,
            {std::max(-pitchGroupSize, -lo), std::min(pitchGroupSize, totalTones - 1 - hi)},
            [this](int rows) { transposeRows(rows); });
    };
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

    if (!pattern || patternId < 0) return;

    // Bail on a disabled/invalid slot before mutating anything.
    int virtualPos = rowOffset + numRows - 1 - visualRow;
    int abs_row    = virtualToAbsRow(virtualPos);
    if (abs_row < 0) return;

    float col = float(absCol);

    // Clear any other note that starts in this same column (same start time),
    // on any row, so a column holds at most one note-start.
    // Collect ids first: removeNote rebuilds `notes`, invalidating iterators.
    std::vector<int> sameColumn;
    for (const auto& n : notes) {
        if ((int)std::floor(n.beat) == absCol)
            sameColumn.push_back(n.id);
    }
    for (int id : sameColumn)
        pattern->removeNote(id);

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
// Dark if that position is the bottom of a pitch group (virtualPos % pitchGroupSize == 0).
Fl_Color PatternGrid::rowLineColor(int i) const
{
    if (i <= 0 || i >= numRows || pitchGroupSize <= 0) return 0xEE888800;
    if ((rowOffset + numRows - i) % pitchGroupSize == 0)
        return 0x33110000;  // dark pitch-group boundary
    return 0xEE888800;
}

// Grey background for every disabled slot, in every pitch group — not just the
// ones a disabled note happens to sit in. The disabled degrees belong to the
// layout, so they read as continuous stripes running the height of the grid.
Fl_Color PatternGrid::rowBgColor(int row) const
{
    if (pitchGroupSize <= 0) return bgColor;
    int virtualPos = rowOffset + numRows - 1 - row;
    if (virtualPos < 0 || (totalTones > 0 && virtualPos >= totalTones)) return bgColor;
    return virtualToAbsRow(virtualPos) < 0 ? 0xCCCCCC00 : bgColor;
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
