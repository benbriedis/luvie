// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "pianorollGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>

PianorollGrid::PianorollGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
    : Grid(numRows, numCols, rowHeight, colWidth, snap, popup)
{}

PianorollGrid::~PianorollGrid()
{
    swapObserver(pattern, nullptr, this);
}

void PianorollGrid::setPattern(ObservablePattern* tl, int patId)
{
    swapObserver(pattern, tl, this);
    patternId = patId;
    rebuildNotes();
    redraw();
}

void PianorollGrid::rebuildNotes()
{
    notes.clear();
    if (!pattern || patternId < 0) { clampSelection(); return; }

    auto patNotes = pattern->buildPatternNotes(patternId);
    for (auto n : patNotes) {
        int visual = (rowOffset + numRows - 1) - n.row;
        if (visual < 0 || visual >= numRows) continue;
        n.row = visual;
        notes.push_back(n);
    }

    clampSelection();
}

void PianorollGrid::onTimelineChanged()
{
    if (!isActiveDrag())
        rebuildNotes();
    redraw();
}

void PianorollGrid::toggleNote()
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

    int midiNote = rowOffset + numRows - 1 - visual_row;
    if (midiNote < 0 || midiNote >= totalRows) return;

    float col    = newNoteStart(fcol);
    float length = newNoteLength();

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == visual_row
                                  && beatsOverlap(col, length, n.beat, n.length); });
    if (clear)
        pattern->addNote(patternId, col, midiNote, length);
}

std::function<void()> PianorollGrid::makeDeleteCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { pattern->removeNote(id); };
}

std::function<void(float)> PianorollGrid::makeVelocityCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id](float v) { pattern->setNoteVelocity(id, v); };
}

// ---------------------------------------------------------------------------
// Multi-selection. A visual row is one semitone, and the grid is drawn highest
// pitch first, so moving DOWN the screen by dRow rows means SUBTRACTING dRow
// from the MIDI note. Everything below reads the pattern rather than `notes`,
// which holds only the rows currently on screen.
// ---------------------------------------------------------------------------

std::unordered_set<int> PianorollGrid::liveItemIds() const
{
    std::unordered_set<int> ids;
    if (!pattern || patternId < 0) return ids;
    for (const Note& n : pattern->buildPatternNotes(patternId)) ids.insert(n.id);
    return ids;
}

void PianorollGrid::selectAll()
{
    selection.clear();
    if (!pattern || patternId < 0) return;
    for (const Note& n : pattern->buildPatternNotes(patternId)) selection.add(n.id);
}

void PianorollGrid::deleteSelection()
{
    if (!pattern || selection.empty()) return;
    std::vector<int> doomed(selection.ids().begin(), selection.ids().end());
    selection.clear();
    ObservableSong::Batch batch(pattern->song());
    for (int id : doomed) pattern->removeNote(id);
}

void PianorollGrid::groupDragLimits(float& minDBeat, float& maxDBeat,
                                    int& minDRow, int& maxDRow) const
{
    minDBeat = maxDBeat = 0.0f;
    minDRow  = maxDRow  = 0;
    if (!pattern || patternId < 0) return;

    bool first = true;
    for (const Note& n : pattern->buildPatternNotes(patternId)) {
        if (!selection.contains(n.id)) continue;
        // Beat: the note must stay inside [0, numCols].
        float bLo = -n.beat;
        float bHi = (float)numCols - (n.beat + n.length);
        // Pitch: 0 <= n.row - dRow <= 127.
        int rLo = n.row - (totalRows - 1);
        int rHi = n.row;
        if (first) { minDBeat = bLo; maxDBeat = bHi; minDRow = rLo; maxDRow = rHi; first = false; }
        else {
            minDBeat = std::max(minDBeat, bLo);
            maxDBeat = std::min(maxDBeat, bHi);
            minDRow  = std::max(minDRow,  rLo);
            maxDRow  = std::min(maxDRow,  rHi);
        }
    }
}

bool PianorollGrid::groupMoveBlocked(float dBeat, int dRow) const
{
    if (!pattern || patternId < 0) return false;
    auto all = pattern->buildPatternNotes(patternId);
    for (const Note& n : all) {
        if (!selection.contains(n.id)) continue;
        const float beat = n.beat + dBeat;
        const int   midi = n.row  - dRow;
        for (const Note& other : all) {
            // Selected notes all move by the same delta, so their relative
            // geometry is unchanged and they cannot newly collide with one
            // another. Only unselected notes can be in the way.
            if (selection.contains(other.id)) continue;
            if (other.row != midi) continue;
            if (beatsOverlap(beat, n.length, other.beat, other.length)) return true;
        }
    }
    return false;
}

void PianorollGrid::onCommitGroupMove(float dBeat, int dRow)
{
    if (!pattern || patternId < 0) return;
    std::vector<Note> sel;
    for (const Note& n : pattern->buildPatternNotes(patternId))
        if (selection.contains(n.id)) sel.push_back(n);

    ObservableSong::Batch batch(pattern->song());
    for (const Note& n : sel)
        pattern->moveNote(n.id, n.beat + dBeat, (float)(n.row - dRow));
}

// A copy carries the visual row rather than the MIDI note, so the paste lands
// under the cursor rather than back at the pitch it came from. rowOffset cancels
// out once the items are rebased, so scrolling between copy and paste changes
// nothing.
std::vector<ClipItem> PianorollGrid::selectedForClipboard() const
{
    std::vector<ClipItem> items;
    if (!pattern || patternId < 0) return items;
    for (const Note& n : pattern->buildPatternNotes(patternId))
        if (selection.contains(n.id))
            items.push_back({(rowOffset + numRows - 1) - n.row, n.beat, n.length, n.velocity, 0.0f});
    return items;
}

bool PianorollGrid::pasteAt(const std::vector<ClipItem>& items, int visualRow, float beat)
{
    if (!pattern || patternId < 0 || items.empty()) return false;

    struct Place { int midi; float beat, length, velocity; };
    std::vector<Place> places;
    places.reserve(items.size());
    for (const auto& it : items) {
        int midi = rowOffset + numRows - 1 - (visualRow + it.dRow);
        if (midi < 0 || midi >= totalRows) return false;
        float b = beat + it.dBeat;
        if (b < 0.0f || b + it.length > (float)numCols) return false;
        places.push_back({midi, b, it.length, it.velocity});
    }

    // Every note the pattern already holds is in the way, the copied ones
    // included: nothing is vacating its place, unlike a group move.
    auto all = pattern->buildPatternNotes(patternId);
    for (const auto& p : places)
        for (const Note& n : all)
            if (n.row == p.midi && beatsOverlap(p.beat, p.length, n.beat, n.length))
                return false;

    std::vector<int> pasted;
    pasted.reserve(places.size());
    {
        ObservableSong::Batch batch(pattern->song());
        for (const auto& p : places)
            pasted.push_back(pattern->addNote(patternId, p.beat, p.midi, p.length, p.velocity));
    }
    // The copies take the selection over from the notes they were made from,
    // once the batch has closed and they are in the model to be selected.
    selection.clear();
    for (int id : pasted) if (id > 0) selection.add(id);
    redraw();
    return true;
}

void PianorollGrid::onCommitMove(const StateDragMove& s)
{
    if (!pattern) return;
    int id       = notes[s.noteIdx].id;
    int midiNote = rowOffset + numRows - 1 - (int)notes[s.noteIdx].row;
    pattern->moveNote(id, notes[s.noteIdx].beat, (float)midiNote);
}

void PianorollGrid::onCommitResize(const StateDragResize& s)
{
    if (!pattern) return;
    int id = notes[s.noteIdx].id;
    if (s.side == Side::Left)
        pattern->resizeNoteLeft(id, notes[s.noteIdx].beat, notes[s.noteIdx].length);
    else
        pattern->resizeNoteRight(id, notes[s.noteIdx].length);
}

void PianorollGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    rebuildNotes();
    redraw();
}

// Octave boundaries every 12 semitones (bottom of group = multiple of 12 from rowOffset)
Fl_Color PianorollGrid::rowLineColor(int i) const
{
    if (i <= 0 || i >= numRows) return 0xEE888800;
    if ((rowOffset + numRows - i) % 12 == 0)
        return 0x33110000;
    return 0xEE888800;
}

// Subtle grey tint for black-key rows
Fl_Color PianorollGrid::rowBgColor(int row) const
{
    int midiNote = rowOffset + numRows - 1 - row;
    int semitone = ((midiNote % 12) + 12) % 12;
    static constexpr bool isBlack[12] = {false,true,false,true,false,false,true,false,true,false,true,false};
    return isBlack[semitone] ? 0xE8E8E800 : bgColor;
}

Fl_Color PianorollGrid::columnColor(int col) const
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
