#include "patternGrid.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>

PatternGrid::PatternGrid(std::vector<Note> notes, int numRows, int numCols,
                         int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{}

PatternGrid::~PatternGrid()
{
    if (timeline) timeline->removeObserver(this);
}

void PatternGrid::setTimeline(ObservableTimeline* tl, int patId)
{
    if (timeline) timeline->removeObserver(this);
    timeline  = tl;
    patternId = patId;
    if (timeline) timeline->addObserver(this);
    rebuildNotes();
    redraw();
}

void PatternGrid::rebuildNotes()
{
    if (timeline && patternId >= 0)
        notes = timeline->buildPatternNotes(patternId);
}

void PatternGrid::onTimelineChanged()
{
    if (!isDragging)
        rebuildNotes();
    redraw();
}

void PatternGrid::toggleNote()
{
    int   ex  = Fl::event_x() - x();
    int   ey  = Fl::event_y() - y();
    int   row = ey / rowHeight;
    float col = (float)(ex / colWidth);

    if (!timeline || patternId < 0) {
        Grid::toggleNote();
        return;
    }

    for (auto& n : notes) {
        if (n.row == row && n.col == col) {
            timeline->removeNote(n.id);
            return;
        }
    }
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == row && col < n.col + n.length && col + 1.0f > n.col; });
    if (clear)
        timeline->addNote(patternId, col, (float)row, 1.0f);
}

std::function<void()> PatternGrid::makeDeleteCallback()
{
    if (!timeline) return nullptr;
    int id = notes[selectedNote].id;
    return [this, id]() { timeline->removeNote(id); };
}

void PatternGrid::onBeginDrag()
{
    draggingNoteId = notes[selectedNote].id;
    isDragging     = true;
}

void PatternGrid::onCommitDrag()
{
    if (!timeline || draggingNoteId < 0) return;
    isDragging = false;  // clear before timeline call so onTimelineChanged can rebuild
    if (hoverState == MOVING)
        timeline->moveNote(draggingNoteId, notes[selectedNote].col, (float)notes[selectedNote].row);
    else if (hoverState == RESIZING) {
        if (side == LEFT)
            timeline->resizeNoteLeft(draggingNoteId, notes[selectedNote].col, notes[selectedNote].length);
        else
            timeline->resizeNoteRight(draggingNoteId, notes[selectedNote].length);
    }
    draggingNoteId = -1;
}

Fl_Color PatternGrid::columnColor(int col) const
{
    if (!displayTl) return 0x00EE0000;
    int queryBar = playhead ? (int)playhead->currentBar() : 0;
    int top, bottom;
    displayTl->timeSigAt(queryBar, top, bottom);
    int beatsPerBar = top;
    bool isBarStart = beatsPerBar > 0 && col % beatsPerBar == 0;
    return isBarStart ? 0x00660000 : 0x00EE0000;
}
