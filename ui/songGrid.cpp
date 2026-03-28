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
        int y0  = y() + note.pitch * rowHeight;
        int top = 4, bottom = 4;
        if (timeline) timeline->timeSigAt((int)note.beat, top, bottom);

        float startOffset = 0.0f;
        // While left-resizing, show dragStartOffset so the tick stays fixed visually
        if (auto* s = std::get_if<StateDragResize>(&state))
            if (s->side == LEFT && note.id == notes[s->noteIdx].id)
                startOffset = dragStartOffset;
        if (startOffset == 0.0f)
            if (const PatternInstance* inst = timeline->instanceById(note.id))
                startOffset = inst->startOffset;

        float beatZeroPos = note.beat - startOffset / top;
        float instanceEnd = note.beat + note.length;

        float intervalBars = 0.0f;
        if (const Pattern* pat = timeline->patternForInstance(note.id))
            if (pat->lengthBeats > 0.0f)
                intervalBars = pat->lengthBeats / top;

        float firstTick = beatZeroPos;
        if (intervalBars > 0.0f) {
            float k = std::ceil((note.beat - beatZeroPos) / intervalBars);
            firstTick = beatZeroPos + k * intervalBars;
        }

        fl_color(FL_WHITE);
        for (float tickBar = firstTick; tickBar < instanceEnd; ) {
            int tickX = x() + (int)((tickBar - colOffset) * colWidth);
            fl_rectf(tickX, y0 + 1,                     2, tickH);
            fl_rectf(tickX, y0 + rowHeight - 1 - tickH, 2, tickH);
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
    if (!isActiveDrag())
        rebuildNotes();
    redraw();
}

std::function<void()> SongGrid::makeDeleteCallback(int noteIdx)
{
    if (!timeline) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { timeline->removePattern(id); };
}

void SongGrid::onBeginDrag(int noteIdx)
{
    if (timeline) { int dummy; timeline->timeSigAt((int)notes[noteIdx].beat, dragBeatsPerBar, dummy); }
    float startOffset = 0.0f;
    if (const PatternInstance* inst = timeline->instanceById(notes[noteIdx].id))
        startOffset = inst->startOffset;
    tickBarPos = notes[noteIdx].beat - startOffset / dragBeatsPerBar;
}

void SongGrid::resizing(StateDragResize& s)
{
    Grid::resizing(s);
    if (s.side == LEFT) {
        float newOffset = (notes[s.noteIdx].beat - tickBarPos) * dragBeatsPerBar;
        dragStartOffset = newOffset;
    }
}

void SongGrid::onCommitMove(const StateDragMove& s)
{
    if (!timeline) return;
    int id = notes[s.noteIdx].id;
    timeline->movePattern(id, (int)notes[s.noteIdx].pitch + rowOffset, notes[s.noteIdx].beat);
}

void SongGrid::onCommitResize(const StateDragResize& s)
{
    if (!timeline) return;
    int id = notes[s.noteIdx].id;
    if (s.side == LEFT)
        timeline->resizePatternLeft(id, notes[s.noteIdx].beat, notes[s.noteIdx].length, dragStartOffset);
    else
        timeline->resizePattern(id, notes[s.noteIdx].length);
}

void SongGrid::onNoteDoubleClick(int noteIdx)
{
    if (onPatternDoubleClick)
        onPatternDoubleClick((int)notes[noteIdx].pitch + rowOffset);
}

void SongGrid::toggleNote()
{
    if (trackFilter >= 0) return;
    int   ex        = Fl::event_x() - x();
    int   ey        = Fl::event_y() - y();
    int   visualRow = ey / rowHeight;
    int   absRow    = visualRow + rowOffset;
    float col       = (float)(ex / colWidth) + colOffset;

    if (!timeline) { Grid::toggleNote(); return; }

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
