#include "songGrid.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <algorithm>

SongGrid::SongGrid(std::vector<Note> notes, int numRows, int numCols,
                   int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{}

SongGrid::~SongGrid()
{
    if (timeline) timeline->removeObserver(this);
}

void SongGrid::setTimeline(ObservableTimeline* tl)
{
    if (timeline) timeline->removeObserver(this);
    timeline = tl;
    if (timeline) {
        timeline->addObserver(this);
        rebuildNotes();
        redraw();
    }
}

void SongGrid::rebuildNotes()
{
    if (!timeline) return;
    if (trackFilter < 0) {
        notes = timeline->buildNotes();
        return;
    }
    const auto& tracks = timeline->get().tracks;
    if (trackFilter >= (int)tracks.size()) { notes.clear(); return; }
    float scale = 1.0f;
    if (beatResolution) {
        int top, bottom;
        timeline->timeSigAt(0, top, bottom);
        scale = (float)top;
    }
    notes.clear();
    for (auto& p : tracks[trackFilter].patterns)
        notes.push_back({p.id, 0, p.startBar * scale, p.length * scale});
}

void SongGrid::setTrackView(int tf, bool br)
{
    trackFilter    = tf;
    beatResolution = br;
    if (timeline) { rebuildNotes(); redraw(); }
}

void SongGrid::onTimelineChanged()
{
    if (!isDragging)
        rebuildNotes();
    redraw();
}

std::function<void()> SongGrid::makeDeleteCallback()
{
    if (!timeline) return nullptr;
    int id = notes[selectedNote].id;
    return [this, id]() { timeline->removePattern(id); };
}

void SongGrid::onBeginDrag()
{
    draggingPatternId = notes[selectedNote].id;
    originalLength    = notes[selectedNote].length;
    isDragging        = true;
}

void SongGrid::onCommitDrag()
{
    if (!timeline || draggingPatternId < 0) return;
    isDragging = false;  // clear before timeline call so onTimelineChanged can rebuild
    if (hoverState == MOVING)
        timeline->movePattern(draggingPatternId, notes[selectedNote].row, notes[selectedNote].col);
    else if (hoverState == RESIZING)
        timeline->resizePattern(draggingPatternId, notes[selectedNote].length);
    draggingPatternId = -1;
}

void SongGrid::toggleNote()
{
    if (trackFilter >= 0) return;  // read-only when displaying a single track view
    int   ex  = Fl::event_x() - x();
    int   ey  = Fl::event_y() - y();
    int   row = ey / rowHeight;
    float col = (float)(ex / colWidth);

    if (!timeline) {
        Grid::toggleNote();
        return;
    }

    for (auto& n : notes) {
        if (n.row == row && n.col == col) {
            timeline->removePattern(n.id);
            return;
        }
    }
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == row && col < n.col + n.length && col + 1.0f > n.col; });
    if (clear)
        timeline->addPattern(row, col, 1.0f);
}
