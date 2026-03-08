#include "songGrid.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>

SongGrid::SongGrid(std::vector<Note> notes, int numRows, int numCols,
                   int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{}

void SongGrid::draw()
{
    Grid::draw();
    if (!timeline) return;

    const int tickH = 4;
    for (const auto& note : notes) {
        if (note.row < 0 || note.row >= numRows) continue;
        int y0 = y() + note.row * rowHeight;
        int top = 4, bottom = 4;
        if (timeline) timeline->timeSigAt((int)note.col, top, bottom);
        float tickBarPos = note.col + note.startOffset / top;
        int   tickX      = x() + (int)(tickBarPos * colWidth);
        fl_color(FL_WHITE);
        fl_rectf(tickX, y0 + 1,                      2, tickH);
        fl_rectf(tickX, y0 + rowHeight - 1 - tickH,  2, tickH);
    }
}

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
    if (timeline) { int dummy; timeline->timeSigAt((int)notes[selectedNote].col, dragBeatsPerBar, dummy); }
    // Capture absolute song-bar position of the pattern's beat-0 tick.
    tickBarPos = notes[selectedNote].col + notes[selectedNote].startOffset / dragBeatsPerBar;
}

void SongGrid::resizing()
{
    Grid::resizing();
    if (side == LEFT) {
        // Keep the beat-0 tick fixed in song position.
        float newOffset = (tickBarPos - notes[selectedNote].col) * dragBeatsPerBar;
        notes[selectedNote].startOffset = std::max(0.0f, newOffset);
    }
}

void SongGrid::onCommitDrag()
{
    if (!timeline || draggingPatternId < 0) return;
    isDragging = false;  // clear before timeline call so onTimelineChanged can rebuild
    if (hoverState == MOVING)
        timeline->movePattern(draggingPatternId, notes[selectedNote].row, notes[selectedNote].col);
    else if (hoverState == RESIZING) {
        if (side == LEFT)
            timeline->resizePatternLeft(draggingPatternId,
                                        notes[selectedNote].col,
                                        notes[selectedNote].length,
                                        notes[selectedNote].startOffset);
        else
            timeline->resizePattern(draggingPatternId, notes[selectedNote].length);
    }
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
