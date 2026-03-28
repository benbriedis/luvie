#include "patternGrid.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>

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

void PatternGrid::rebuildNotes()
{
    notes.clear();
    if (!timeline || patternId < 0) { clampSelection(); return; }
    for (auto n : timeline->buildPatternNotes(patternId)) {
        int visual = (rowOffset + numRows - 1) - (int)n.pitch;
        if (visual >= 0 && visual < numRows) {
            n.pitch = visual;
            notes.push_back(n);
        }
    }
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
    int   ey         = Fl::event_y() - y();
    int   ex         = Fl::event_x() - x();
    int   gridRight  = std::min(w(), (numCols - colOffset) * colWidth);
    if (ex >= gridRight) return;

    int   visual_row = ey / rowHeight;
    int   abs_row    = (rowOffset + numRows - 1) - visual_row;
    float col        = (float)(ex / colWidth) + colOffset;

    if (!timeline || patternId < 0) {
        Grid::toggleNote();
        return;
    }

    for (auto& n : notes) {
        if (n.pitch == visual_row && n.beat == col) {
            timeline->removeNote(n.id);
            return;
        }
    }
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.pitch == visual_row && col < n.beat + n.length && col + 1.0f > n.beat; });
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
    if (!timeline) return;
    int   id      = notes[s.noteIdx].id;
    float abs_row = (float)((rowOffset + numRows - 1) - (int)notes[s.noteIdx].pitch);
    timeline->moveNote(id, notes[s.noteIdx].beat, abs_row);
}

void PatternGrid::onCommitResize(const StateDragResize& s)
{
    if (!timeline) return;
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

Fl_Color PatternGrid::rowLineColor(int i) const
{
    if (i > 0 && i < numRows && chordSize > 0 && (numRows - i) % chordSize == 0)
        return 0x33110000;  // dark octave boundary
    return 0xEE888800;
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
