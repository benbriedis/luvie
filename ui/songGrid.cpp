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

    fl_push_clip(x(), y(), w(), h());
    const int tickH = 4;
    for (const auto& note : notes) {
        if (note.pitch < 0 || note.pitch >= numRows) continue;
        int   y0     = y() + note.pitch * rowHeight;
        int   top    = 4, bottom = 4;
        if (timeline) timeline->timeSigAt((int)note.beat, top, bottom);

        float startOffset = 0.0f;
        if (isDragging && hoverState == RESIZING && side == LEFT && note.id == draggingPatternId)
            startOffset = dragStartOffset;
        else if (const PatternInstance* inst = timeline->instanceById(note.id))
            startOffset = inst->startOffset;
        float beatZeroPos = note.beat - startOffset / top;
        float instanceEnd = note.beat + note.length;

        float intervalBars = 0.0f;
        if (timeline) {
            const Pattern* pat = timeline->patternForInstance(note.id);
            if (pat && pat->lengthBeats > 0.0f)
                intervalBars = pat->lengthBeats / top;
        }

        // Find the first tick >= note.beat in the series beatZeroPos + k*intervalBars.
        // ceil() handles both cases: beatZeroPos before or inside the instance.
        float firstTick = beatZeroPos;
        if (intervalBars > 0.0f) {
            float k = std::ceil((note.beat - beatZeroPos) / intervalBars);
            firstTick = beatZeroPos + k * intervalBars;
        }

        fl_color(FL_WHITE);
        for (float tickBar = firstTick; tickBar < instanceEnd; ) {
            int tickX = x() + (int)((tickBar - colOffset) * colWidth);
            fl_rectf(tickX, y0 + 1,                      2, tickH);
            fl_rectf(tickX, y0 + rowHeight - 1 - tickH,  2, tickH);
            if (intervalBars <= 0.0f) break;
            tickBar += intervalBars;
        }
    }
    fl_pop_clip();
}

SongGrid::~SongGrid()
{
    swapObserver(timeline, nullptr, this);
}

void SongGrid::setTimeline(ObservableTimeline* tl)
{
    swapObserver(timeline, tl, this);
    rebuildNotes();
    redraw();
}

void SongGrid::rebuildNotes()
{
    if (!timeline) return;
    if (trackFilter >= 0) {
        // Single-track view: no row offset needed
        const auto& tracks = timeline->get().tracks;
        notes.clear();
        if (trackFilter < (int)tracks.size()) {
            float scale = 1.0f;
            if (beatResolution) {
                int top, bottom;
                timeline->timeSigAt(0, top, bottom);
                scale = (float)top;
            }
            for (auto& p : tracks[trackFilter].patterns)
                notes.push_back({p.id, 0, p.startBar * scale, p.length * scale});
        }
        clampSelection();
        return;
    }
    // All-tracks view: map absolute track index to visual row
    std::vector<Note> all = timeline->buildNotes();
    notes.clear();
    for (auto n : all) {
        int visual = (int)n.pitch - rowOffset;
        if (visual >= 0 && visual < numRows) {
            n.pitch = (float)visual;
            notes.push_back(n);
        }
    }
    clampSelection();
}

void SongGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    if (timeline) { rebuildNotes(); redraw(); }
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
    if (timeline) { int dummy; timeline->timeSigAt((int)notes[selectedNote].beat, dragBeatsPerBar, dummy); }
    // Capture absolute song-bar position of the pattern's beat-0 tick.
    float startOffset = 0.0f;
    if (const PatternInstance* inst = timeline->instanceById(notes[selectedNote].id))
        startOffset = inst->startOffset;
    tickBarPos = notes[selectedNote].beat - startOffset / dragBeatsPerBar;
}

void SongGrid::resizing()
{
    Grid::resizing();
    if (side == LEFT) {
        // Keep the beat-0 tick fixed in song position.
        float newOffset = (notes[selectedNote].beat - tickBarPos) * dragBeatsPerBar;
        dragStartOffset = newOffset;
    }
}

void SongGrid::onCommitDrag()
{
    if (!timeline || draggingPatternId < 0) return;
    isDragging = false;  // clear before timeline call so onTimelineChanged can rebuild
    if (hoverState == MOVING)
        timeline->movePattern(draggingPatternId, (int)notes[selectedNote].pitch + rowOffset, notes[selectedNote].beat);
    else if (hoverState == RESIZING) {
        if (side == LEFT)
            timeline->resizePatternLeft(draggingPatternId,
                                        notes[selectedNote].beat,
                                        notes[selectedNote].length,
                                        dragStartOffset);
        else
            timeline->resizePattern(draggingPatternId, notes[selectedNote].length);
    }
    draggingPatternId = -1;
}

void SongGrid::onNoteDoubleClick()
{
    if (selectedNote < (int)notes.size() && onPatternDoubleClick)
        onPatternDoubleClick((int)notes[selectedNote].pitch + rowOffset);
}

void SongGrid::toggleNote()
{
    if (trackFilter >= 0) return;  // read-only when displaying a single track view
    int   ex       = Fl::event_x() - x();
    int   ey       = Fl::event_y() - y();
    int   visualRow = ey / rowHeight;
    int   absRow    = visualRow + rowOffset;
    float col       = (float)(ex / colWidth) + colOffset;

    if (!timeline) {
        Grid::toggleNote();
        return;
    }

    for (auto& n : notes) {
        if ((int)n.pitch == visualRow && n.beat == col) {
            timeline->removePattern(n.id);
            return;
        }
    }
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return (int)n.pitch == visualRow && col < n.beat + n.length && col + 1.0f > n.beat; });
    const auto& tracks = timeline->get().tracks;
    if (clear && absRow >= 0 && absRow < (int)tracks.size()) {
        int patId = tracks[absRow].patternId;
        if (patId > 0)
            timeline->placePattern(absRow, patId, col, 1.0f);
    }
}
