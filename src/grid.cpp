#include "grid.hpp"
#include "playhead.hpp"
#include "editor.hpp"
#include <FL/Fl.H>
#include "FL/Enumerations.H"
#include "popup.hpp"
#include <ranges>
#include <algorithm>
#include <cmath>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_RGB_Image.H>
#include "cursors.hpp"
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>

using std::vector;

Grid::Grid(vector<Note> notes, int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup) :
    notes(notes), numRows(numRows), numCols(numCols), rowHeight(rowHeight), colWidth(colWidth), snap(snap), popup(popup),
    Fl_Box(0, 0, numCols * colWidth, numRows * rowHeight, nullptr)
{}

void Grid::draw()
{
    Fl_Box::draw();

    fl_push_clip(x(), y(), w() + 1, h() + 1);

    fl_color(bgColor);
    fl_rectf(x(), y(), w(), h());

    int gridRight = std::min(w(), (numCols - colOffset) * colWidth);

    for (int r = 0; r < numRows; r++) {
        Fl_Color rc = rowBgColor(r);
        if (rc != bgColor) {
            fl_color(rc);
            fl_rectf(x(), y() + r * rowHeight, gridRight, rowHeight);
        }
    }

    for (int i = 0; i < numRows + 1; i++) {
        fl_color(rowLineColor(i));
        fl_line(x(), y() + i * rowHeight, x() + gridRight, y() + i * rowHeight);
    }

    int endCol = colOffset + w() / colWidth + 2;
    for (int i = colOffset; i <= std::min(endCol, numCols); i++) {
        int x0 = x() + (i - colOffset) * colWidth;
        fl_color(columnColor(i));
        fl_line(x0, y(), x0, y() + numRows * rowHeight);
    }

    for (const Note& note : notes) {
        int x0    = x() + (int)((note.beat - colOffset) * colWidth);
        int y0    = y() + (int)(note.pitch * rowHeight);
        int width = (int)(note.length * colWidth);
        if (x0 + width < x() || x0 > x() + w()) continue;
        Fl_Color fill = 0x5555EE00;
        Fl_Color bar  = 0x1111EE00;
        fl_rectf(x0, y0 + 1, width, rowHeight - 1, fill);
        const int barWidth = 5;
        fl_color(bar);
        fl_line_style(FL_SOLID, barWidth);
        fl_line(x0 + barWidth / 2, y0 + 1, x0 + barWidth / 2, y0 + rowHeight - 1);
        fl_line_style(0);
    }

    if (playhead)
        playhead->drawLine(x() - colOffset * colWidth, y(), numRows * rowHeight);

    fl_pop_clip();
}

int Grid::handle(int event)
{
    if (popup.visible())
        return 0;

    switch (event) {
        case FL_PUSH:
            if (std::holds_alternative<StateIdle>(state))
                findNoteForCursor();
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                int idx = -1;
                if (auto* h = std::get_if<StateHoverMove>  (&state)) idx = h->noteIdx;
                else if (auto* h = std::get_if<StateHoverResize>(&state)) idx = h->noteIdx;
                if (idx >= 0)
                    popup.open(idx, &notes, this, makeDeleteCallback(idx));
            } else if (auto* h = std::get_if<StateHoverMove>(&state)) {
                int   noteIdx = h->noteIdx;
                float grabX   = h->grabX;
                float grabY   = h->grabY;
                if (Fl::event_clicks() == 1) {
                    onNoteDoubleClick(noteIdx);
                } else {
                    Point orig = {(int)notes[noteIdx].pitch, notes[noteIdx].beat};
                    onBeginDrag(noteIdx);
                    state = StateDragMove{noteIdx, grabX, grabY, orig, orig, false};
                }
            } else if (auto* h = std::get_if<StateHoverResize>(&state)) {
                int  noteIdx = h->noteIdx;
                Side side    = h->side;
                onBeginDrag(noteIdx);
                state = StateDragResize{noteIdx, side};
            } else {
                // Idle — check whether note creation is allowed at click position
                int   ex       = Fl::event_x() - x();
                int   row      = (Fl::event_y() - y()) / rowHeight;
                float col      = (float)(ex / colWidth) + colOffset;
                int   gridRight = std::min(w(), (numCols - colOffset) * colWidth);
                creationForbidden = ex >= gridRight;
                if (!creationForbidden) {
                    bool wouldRemove = std::any_of(notes.begin(), notes.end(),
                        [=](const Note& n) { return n.pitch == row && n.beat == col; });
                    if (!wouldRemove) {
                        creationForbidden = std::any_of(notes.begin(), notes.end(),
                            [=](const Note& n) { return n.pitch == row && col < n.beat + n.length && col + 1.0f > n.beat; });
                        if (creationForbidden)
                            window()->cursor(forbiddenCursorImage(), 11, 11);
                    }
                }
            }
            return 1;

        case FL_DRAG:
            if (auto* s = std::get_if<StateDragMove>  (&state)) moving(*s);
            else if (auto* s = std::get_if<StateDragResize>(&state)) resizing(*s);
            return 1;

        case FL_RELEASE:
            if (auto* s = std::get_if<StateDragMove>(&state)) {
                // Capture before clearing state
                bool  wasOverlapping = s->overlapping;
                int   noteIdx        = s->noteIdx;
                Point lastValid      = s->lastValid;
                StateDragMove drag   = *s;
                if (wasOverlapping) {
                    notes[noteIdx].pitch = lastValid.row;
                    notes[noteIdx].beat  = lastValid.col;
                    redraw();
                }
                state = StateIdle{};   // clear BEFORE commit so isActiveDrag() is false
                onCommitMove(drag);
            } else if (auto* s = std::get_if<StateDragResize>(&state)) {
                StateDragResize drag = *s;
                state = StateIdle{};
                onCommitResize(drag);
            } else {
                // Simple click — toggle note
                if (Fl::event_button() == FL_LEFT_MOUSE && !creationForbidden)
                    toggleNote();
                creationForbidden = false;
            }
            window()->cursor(FL_CURSOR_DEFAULT);
            return 1;

        case FL_ENTER:
            return 1;

        case FL_MOVE:
            findNoteForCursor();
            return 0;

        default:
            return 0;
    }
}

void Grid::moving(StateDragMove& s)
{
    Note* note = &notes[s.noteIdx];
    float ex   = Fl::event_x() - x();
    note->beat  = (ex - s.grabX) / (float)colWidth + colOffset;
    if (note->beat < 0.0f) note->beat = 0.0f;
    if (note->beat + note->length > numCols) note->beat = numCols - note->length;
    if (snap > 0.0f) note->beat = std::round(note->beat / snap) * snap;
    float ey    = Fl::event_y() - y();
    note->pitch  = (ey - s.grabY + rowHeight / 2.0f) / (float)rowHeight;
    if (note->pitch < 0)        note->pitch = 0;
    if (note->pitch >= numRows) note->pitch = numRows - 1;
    s.overlapping = overlappingNote(s.noteIdx) >= 0;
    if (!s.overlapping) s.lastValid = {(int)note->pitch, note->beat};
    if (s.overlapping) window()->cursor(forbiddenCursorImage(), 11, 11);
    else               window()->cursor(FL_CURSOR_HAND);
    redraw();
}

void Grid::resizing(StateDragResize& s)
{
    float minLength = 10.0f / colWidth;
    Note* note      = &notes[s.noteIdx];
    float ex        = Fl::event_x() - x();
    if (s.side == LEFT) {
        float endCol = note->beat + note->length;
        note->beat    = ex / (float)colWidth + colOffset;
        if (snap) note->beat = std::round(note->beat / snap) * snap;
        int   neighbour = overlappingNote(s.noteIdx);
        float min       = neighbour < 0 ? 0.0f : notes[neighbour].beat + notes[neighbour].length;
        if (note->beat < min) note->beat = min;
        note->length = endCol - note->beat;
        if (note->length < minLength) { note->length = minLength; note->beat = endCol - minLength; }
    } else {
        note->length  = ex / (float)colWidth + colOffset - note->beat;
        float endCol  = note->beat + note->length;
        if (snap) { endCol = std::round(endCol / snap) * snap; note->length = endCol - note->beat; }
        int   neighbour = overlappingNote(s.noteIdx);
        float max       = neighbour < 0 ? (float)numCols : notes[neighbour].beat;
        if (note->beat + note->length > max) note->length = max - note->beat;
        if (note->length < minLength) note->length = minLength;
    }
    redraw();
}

void Grid::findNoteForCursor()
{
    const int resizeZone = 5;
    float ex  = Fl::event_x() - x();
    int   ey  = Fl::event_y() - y();
    int   row = ey / rowHeight;

    int  resizeIdx  = -1;
    Side resizeSide = LEFT;

    for (const auto [i, n] : std::views::enumerate(notes)) {
        if ((int)n.pitch != row) continue;
        float leftEdge  = (n.beat - colOffset) * colWidth;
        float rightEdge = (n.beat + n.length - colOffset) * colWidth;

        if (leftEdge - ex <= resizeZone && ex - leftEdge <= resizeZone) {
            resizeIdx = i; resizeSide = LEFT;
        } else if (rightEdge - ex <= resizeZone && ex - rightEdge <= resizeZone) {
            resizeIdx = i; resizeSide = RIGHT;
        } else if (ex >= leftEdge && ex <= rightEdge) {
            state = StateHoverMove{(int)i, ex - leftEdge, ey - (int)n.pitch * rowHeight};
            window()->cursor(FL_CURSOR_HAND);
            redraw();
            return;
        }
    }

    if (resizeIdx >= 0) {
        state = StateHoverResize{resizeIdx, resizeSide};
        window()->cursor(FL_CURSOR_WE);
    } else {
        state = StateIdle{};
        window()->cursor(FL_CURSOR_DEFAULT);
    }
    redraw();
}

void Grid::toggleNote()
{
    int   ex  = Fl::event_x() - x();
    int   ey  = Fl::event_y() - y();
    int   row = ey / rowHeight;
    float col = (float)(ex / colWidth) + colOffset;

    int size = notes.size();
    notes.erase(std::remove_if(notes.begin(), notes.end(),
        [=](const Note& n) { return n.pitch == row && n.beat == col; }), notes.end());
    if ((int)notes.size() == size) {
        bool clear = std::none_of(notes.begin(), notes.end(),
            [=](const Note& n) { return n.pitch == row && col < n.beat + n.length && col + 1.0f > n.beat; });
        if (clear)
            notes.push_back({0, row, col, 1.0});
    }
    redraw();
}

int Grid::overlappingNote(int noteIdx) const
{
    Note  a      = notes[noteIdx];
    float aStart = a.beat, aEnd = a.beat + a.length;
    for (const auto [i, b] : std::views::enumerate(notes)) {
        if (i == noteIdx || b.pitch != a.pitch) continue;
        float bStart      = b.beat, bEnd = b.beat + b.length;
        float firstEnd    = aStart <= bStart ? aEnd : bEnd;
        float secondStart = aStart <= bStart ? bStart : aStart;
        if (firstEnd > secondStart) return i;
    }
    return -1;
}

void Grid::clampSelection()
{
    int sz = (int)notes.size();
    auto oob = [sz](int i) { return i < 0 || i >= sz; };
    if      (auto* s = std::get_if<StateHoverMove>  (&state)) { if (oob(s->noteIdx)) state = StateIdle{}; }
    else if (auto* s = std::get_if<StateHoverResize>(&state)) { if (oob(s->noteIdx)) state = StateIdle{}; }
    else if (auto* s = std::get_if<StateDragMove>   (&state)) { if (oob(s->noteIdx)) state = StateIdle{}; }
    else if (auto* s = std::get_if<StateDragResize> (&state)) { if (oob(s->noteIdx)) state = StateIdle{}; }
}
