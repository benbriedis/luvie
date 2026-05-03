#include "pianorollGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>

PianorollGrid::PianorollGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup)
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
        int visual = (rowOffset + numRows - 1) - n.pitch;
        if (visual < 0 || visual >= numRows) continue;
        n.pitch = visual;
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
    float col        = (float)(ex / colWidth) + colOffset;

    if (!pattern || patternId < 0) {
        Grid::toggleNote();
        return;
    }

    for (auto& n : notes) {
        if (n.pitch == visual_row && n.beat == col) {
            pattern->removeNote(n.id);
            return;
        }
    }

    int midiNote = rowOffset + numRows - 1 - visual_row;
    if (midiNote < 0 || midiNote >= totalRows) return;

    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.pitch == visual_row
                                  && col < n.beat + n.length
                                  && col + 1.0f > n.beat; });
    if (clear)
        pattern->addNote(patternId, col, midiNote, 1.0f);
}

std::function<void()> PianorollGrid::makeDeleteCallback(int noteIdx)
{
    if (!pattern) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { pattern->removeNote(id); };
}

void PianorollGrid::onCommitMove(const StateDragMove& s)
{
    if (!pattern) return;
    int id       = notes[s.noteIdx].id;
    int midiNote = rowOffset + numRows - 1 - (int)notes[s.noteIdx].pitch;
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
