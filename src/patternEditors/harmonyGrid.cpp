// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "harmonyGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <cmath>
#include <set>

HarmonyGrid::HarmonyGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
    : Grid(numRows, numCols, rowHeight, colWidth, snap, popup)
{}

HarmonyGrid::~HarmonyGrid()
{
    swapObserver(pattern, nullptr, this);
}

void HarmonyGrid::setPattern(ObservablePattern* tl, int patId)
{
    swapObserver(pattern, tl, this);
    patternId = patId;
    rebuildNotes();
    redraw();
}

// Virtual row layout per pitch group (pitchGroupSize rows, bottom→top):
//   positions 0..chordSize-1             : the chord's own degrees 0..chordSize-1
//   positions chordSize..pitchGroupSize-1: bonus rows (sorted ascending by bonusDegree)
//
// virtualPos = pitchGroup * pitchGroupSize + posInGroup
// visual row = rowOffset + numRows - 1 - virtualPos   (top of grid = highest pitch)

int HarmonyGrid::virtualToAbsRow(int virtualPos) const
{
    if (pitchGroupSize <= 0) return virtualPos;
    int gs = pitchGroupSize;
    int pos = ((virtualPos % gs) + gs) % gs;
    if (pos >= chordSize) return -1;  // bonus row
    int pitchGroup = virtualPos / gs;
    return pitchGroup * chordSize + pos;
}

// Every row the labels show is a legal home for a note — the chord's own degrees
// and the bonus rows alike. Only rows outside the labelled range are refused.
bool HarmonyGrid::validVirtualPos(int virtualPos) const
{
    if (pitchGroupSize <= 0 || chordSize <= 0) return false;
    if (virtualPos < 0) return false;
    return totalTones <= 0 || virtualPos < totalTones;
}

void HarmonyGrid::rebuildNotes()
{
    notes.clear();
    if (!pattern || patternId < 0) { clampSelection(); return; }

    auto patNotes = pattern->buildPatternNotes(patternId);

    // Recompute the bonus degrees from the current pattern
    std::set<int> bonusSet;
    for (const auto& n : patNotes)
        if (n.bonus && n.bonusDegree >= 0)
            bonusSet.insert(n.bonusDegree);

    std::vector<int> newBonus(bonusSet.begin(), bonusSet.end());  // sorted ascending
    int newPitchGroupSize = chordSize + (int)newBonus.size();

    if (newBonus != bonusDegrees || newPitchGroupSize != pitchGroupSize) {
        bonusDegrees   = newBonus;
        pitchGroupSize = newPitchGroupSize;
    }

    for (auto n : patNotes) {
        int virtualPos;
        if (n.bonus) {
            int pitchGroup = n.row;
            auto it = std::find(bonusDegrees.begin(), bonusDegrees.end(), n.bonusDegree);
            if (it == bonusDegrees.end()) continue;
            int bIdx = (int)std::distance(bonusDegrees.begin(), it);
            virtualPos = pitchGroup * pitchGroupSize + chordSize + bIdx;
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

    if (onBonusDegreesChanged)
        onBonusDegreesChanged(bonusDegrees, pitchGroupSize);

    clampSelection();
}

void HarmonyGrid::onTimelineChanged()
{
    if (!isActiveDrag())
        rebuildNotes();
    redraw();
}

void HarmonyGrid::toggleNote()
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

    int virtualPos = rowOffset + numRows - 1 - visual_row;
    if (!validVirtualPos(virtualPos)) return;

    float col    = newNoteStart(fcol);
    float length = newNoteLength();

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == visual_row
                                  && beatsOverlap(col, length, n.beat, n.length); });
    if (clear)
        addNoteAt(virtualPos, col, length);
}

// Create a note on `virtualPos`, whichever kind of row that turns out to be: a
// bonus row takes a note carrying that row's degree, so it sounds as labelled.
int HarmonyGrid::addNoteAt(int virtualPos, float col, float length, float velocity)
{
    auto slot = slotForVirtualPos(0, virtualPos);
    if (slot.bonus)
        return pattern->addBonusNote(patternId, col, slot.row, slot.bonusDegree, length, velocity);
    return pattern->addNote(patternId, col, slot.row, length, velocity);
}

std::function<void()> HarmonyGrid::makeDeleteCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { pattern->removeNote(id); };
}

std::function<void(float)> HarmonyGrid::makeVelocityCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id](float v) { pattern->setNoteVelocity(id, v); };
}

// Virtual row a stored note occupies — the inverse of the mapping rebuildNotes
// uses. -1 when a bonus note's degree is no longer in the layout.
int HarmonyGrid::virtualPosOf(const Note& n) const
{
    if (pitchGroupSize <= 0 || chordSize <= 0) return -1;
    if (!n.bonus)
        return (n.row / chordSize) * pitchGroupSize + (n.row % chordSize);

    auto it = std::find(bonusDegrees.begin(), bonusDegrees.end(), n.bonusDegree);
    if (it == bonusDegrees.end()) return -1;
    return n.row * pitchGroupSize + chordSize + (int)std::distance(bonusDegrees.begin(), it);
}

// A row's note-slot: a chord degree in the lower part of each pitch group, or one
// of the greyed-out bonus rows above it. A note takes on the character of the row
// it lands on, which is what "move everything up N rows" means visually.
ObservablePattern::NoteRowSlot HarmonyGrid::slotForVirtualPos(int noteId, int virtualPos) const
{
    int pitchGroup = virtualPos / pitchGroupSize;
    int pos        = virtualPos % pitchGroupSize;
    if (pos < chordSize)
        return {noteId, pitchGroup * chordSize + pos, false, -1};
    return {noteId, pitchGroup, true, bonusDegrees[pos - chordSize]};
}

// ---------------------------------------------------------------------------
// Multi-selection. The vertical axis is virtual rows (chord degrees interleaved
// with bonus rows), and the grid draws the highest pitch at the top, so moving
// DOWN the screen by dRow rows SUBTRACTS dRow from the virtual position.
// Dragging a selection vertically is what replaced the Transpose dialog.
// ---------------------------------------------------------------------------

std::unordered_set<int> HarmonyGrid::liveItemIds() const
{
    std::unordered_set<int> ids;
    if (!pattern || patternId < 0) return ids;
    for (const Note& n : pattern->buildPatternNotes(patternId)) ids.insert(n.id);
    return ids;
}

void HarmonyGrid::selectAll()
{
    selection.clear();
    if (!pattern || patternId < 0) return;
    // Bonus notes are ordinary members of the selection; only a note whose
    // degree has fallen out of the layout entirely has nowhere to sit.
    for (const Note& n : pattern->buildPatternNotes(patternId))
        if (virtualPosOf(n) >= 0) selection.add(n.id);
}

void HarmonyGrid::deleteSelection()
{
    if (!pattern || selection.empty()) return;
    std::vector<int> doomed(selection.ids().begin(), selection.ids().end());
    selection.clear();
    ObservableSong::Batch batch(pattern->song());
    for (int id : doomed) pattern->removeNote(id);
}

void HarmonyGrid::groupDragLimits(float& minDBeat, float& maxDBeat,
                                  int& minDRow, int& maxDRow) const
{
    minDBeat = maxDBeat = 0.0f;
    minDRow  = maxDRow  = 0;
    if (!pattern || patternId < 0 || totalTones <= 0) return;

    bool first = true;
    for (const Note& n : pattern->buildPatternNotes(patternId)) {
        if (!selection.contains(n.id)) continue;
        int vp = virtualPosOf(n);
        if (vp < 0) continue;
        float bLo = -n.beat;
        float bHi = (float)numCols - (n.beat + n.length);
        // 0 <= vp - dRow <= totalTones - 1
        int rLo = vp - (totalTones - 1);
        int rHi = vp;
        if (first) { minDBeat = bLo; maxDBeat = bHi; minDRow = rLo; maxDRow = rHi; first = false; }
        else {
            minDBeat = std::max(minDBeat, bLo);
            maxDBeat = std::min(maxDBeat, bHi);
            minDRow  = std::max(minDRow,  rLo);
            maxDRow  = std::min(maxDRow,  rHi);
        }
    }
}

bool HarmonyGrid::groupMoveBlocked(float dBeat, int dRow) const
{
    if (!pattern || patternId < 0) return false;
    auto all = pattern->buildPatternNotes(patternId);
    for (const Note& n : all) {
        if (!selection.contains(n.id)) continue;
        int vp = virtualPosOf(n);
        if (vp < 0) continue;
        const int   destVp = vp - dRow;
        const float beat   = n.beat + dBeat;
        if (!validVirtualPos(destVp)) return true;
        for (const Note& other : all) {
            if (selection.contains(other.id)) continue;
            if (virtualPosOf(other) != destVp) continue;
            if (beatsOverlap(beat, n.length, other.beat, other.length)) return true;
        }
    }
    return false;
}

void HarmonyGrid::onCommitGroupMove(float dBeat, int dRow)
{
    if (!pattern || patternId < 0) return;
    std::vector<ObservablePattern::NoteRowSlot> slots;
    std::vector<float> beats;
    for (const Note& n : pattern->buildPatternNotes(patternId)) {
        if (!selection.contains(n.id)) continue;
        int vp = virtualPosOf(n);
        if (vp < 0) continue;
        int destVp = vp - dRow;
        if (!validVirtualPos(destVp)) return;   // the clamp should have prevented this
        slots.push_back(slotForVirtualPos(n.id, destVp));
        beats.push_back(n.beat + dBeat);
    }
    ObservableSong::Batch batch(pattern->song());
    for (size_t i = 0; i < slots.size(); ++i)
        pattern->moveNoteToSlot(beats[i], slots[i]);
}

// A copy carries the visual row rather than the note's own coordinates, so a
// pasted note takes on the character of the row it lands on — becoming a bonus
// note or an ordinary one — exactly as a dragged one does.
std::vector<ClipItem> HarmonyGrid::selectedForClipboard() const
{
    std::vector<ClipItem> items;
    if (!pattern || patternId < 0) return items;
    for (const Note& n : pattern->buildPatternNotes(patternId)) {
        if (!selection.contains(n.id)) continue;
        int vp = virtualPosOf(n);
        if (vp < 0) continue;   // a bonus degree that has fallen out of the layout
        items.push_back({(rowOffset + numRows - 1) - vp, n.beat, n.length, n.velocity, 0.0f});
    }
    return items;
}

bool HarmonyGrid::pasteAt(const std::vector<ClipItem>& items, int visualRow, float beat)
{
    if (!pattern || patternId < 0 || items.empty()) return false;

    struct Place { int vp; float beat, length, velocity; };
    std::vector<Place> places;
    places.reserve(items.size());
    for (const auto& it : items) {
        int vp = rowOffset + numRows - 1 - (visualRow + it.dRow);
        if (!validVirtualPos(vp)) return false;
        float b = beat + it.dBeat;
        if (b < 0.0f || b + it.length > (float)numCols) return false;
        places.push_back({vp, b, it.length, it.velocity});
    }

    // Every note the pattern already holds is in the way, the copied ones
    // included: nothing is vacating its place, unlike a group move.
    auto all = pattern->buildPatternNotes(patternId);
    for (const auto& p : places)
        for (const Note& n : all)
            if (virtualPosOf(n) == p.vp && beatsOverlap(p.beat, p.length, n.beat, n.length))
                return false;

    // The batch keeps the layout still while the notes go in: a bonus degree
    // that this pattern does not have yet would otherwise resize the pitch
    // groups mid-paste, and the rows resolved above would stop meaning what
    // they meant when they were checked.
    std::vector<int> pasted;
    pasted.reserve(places.size());
    {
        ObservableSong::Batch batch(pattern->song());
        for (const auto& p : places)
            pasted.push_back(addNoteAt(p.vp, p.beat, p.length, p.velocity));
    }
    // The copies take the selection over from the notes they were made from,
    // once the batch has closed and they are in the model to be selected.
    selection.clear();
    for (int id : pasted) if (id > 0) selection.add(id);
    redraw();
    return true;
}

// A dragged note takes on the character of the row it is dropped on, so a note
// dragged onto a bonus row becomes a bonus note, and one dragged off a bonus row
// becomes an ordinary one.
void HarmonyGrid::onCommitMove(const StateDragMove& s)
{
    if (!pattern) return;
    const Note& n  = notes[s.noteIdx];
    int virtualPos = rowOffset + numRows - 1 - (int)n.row;
    if (!validVirtualPos(virtualPos)) return;
    pattern->moveNoteToSlot(n.beat, slotForVirtualPos(n.id, virtualPos));
}

void HarmonyGrid::onCommitResize(const StateDragResize& s)
{
    if (!pattern) return;
    int id = notes[s.noteIdx].id;
    if (s.side == Side::Left)
        pattern->resizeNoteLeft(id, notes[s.noteIdx].beat, notes[s.noteIdx].length);
    else
        pattern->resizeNoteRight(id, notes[s.noteIdx].length);
}

void HarmonyGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    rebuildNotes();
    redraw();
}

void HarmonyGrid::setRapidMode(bool r)
{
    rapidMode           = r;
    rapidRemovedOnClick = false;
    rapidUndo.reset();
    rapidCells.clear();
    rapidLast    = std::nullopt;
    rapidPending = std::nullopt;
    state = StateIdle{};
    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
    redraw();
}

bool HarmonyGrid::screenToCell(int ex, int ey, int& outRow, int& outAbsCol) const
{
    int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
    if (ex < 0 || ex >= gridRight || ey < 0 || ey >= h()) return false;
    outRow    = ey / rowHeight;
    outAbsCol = ex / colWidth + colOffset;
    return true;
}

void HarmonyGrid::rapidTryCreate(int visualRow, int absCol)
{
    if (visualRow < 0 || visualRow >= numRows || absCol < 0 || absCol + 1 > numCols) return;

    auto key = std::make_pair(visualRow, absCol);
    if (rapidCells.count(key)) return;
    rapidCells.insert(key);

    if (!pattern || patternId < 0) return;

    // Bail on a row outside the labelled range before mutating anything.
    int virtualPos = rowOffset + numRows - 1 - visualRow;
    if (!validVirtualPos(virtualPos)) return;

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

    addNoteAt(virtualPos, col, 1.0f);
}

void HarmonyGrid::processRapidCell(RapidCell cur)
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

int HarmonyGrid::handle(int event)
{
    if (!rapidMode)
        return Grid::handle(event);

    switch (event) {
    case FL_PUSH: {
        rapidCells.clear();
        rapidLast    = std::nullopt;
        rapidPending = std::nullopt;
        if (pattern) rapidUndo.emplace(pattern->song());

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
        rapidUndo.reset();
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
Fl_Color HarmonyGrid::rowLineColor(int i) const
{
    if (i <= 0 || i >= numRows || pitchGroupSize <= 0) return 0xEE888800;
    if ((rowOffset + numRows - i) % pitchGroupSize == 0)
        return 0x33110000;  // dark pitch-group boundary
    return 0xEE888800;
}

// Grey background for every bonus row, in every pitch group — not just the ones a
// bonus note happens to sit in. The bonus degrees belong to the layout, so they
// read as continuous stripes running the height of the grid.
Fl_Color HarmonyGrid::rowBgColor(int row) const
{
    if (pitchGroupSize <= 0) return bgColor;
    int virtualPos = rowOffset + numRows - 1 - row;
    if (virtualPos < 0 || (totalTones > 0 && virtualPos >= totalTones)) return bgColor;
    return virtualToAbsRow(virtualPos) < 0 ? 0xCCCCCC00 : bgColor;
}

Fl_Color HarmonyGrid::columnColor(int col) const
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
