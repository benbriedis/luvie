#include "grid.hpp"
#include "playhead.hpp"
#include "editor.hpp"
#include "noteColor.hpp"
#include <FL/Fl.H>
#include "FL/Enumerations.H"
#include "noteContextPopup.hpp"
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

Grid::Grid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup) :
    numRows(numRows), numCols(numCols), rowHeight(rowHeight), colWidth(colWidth), snap(snap), popup(popup),
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
            fl_rectf(x(), y() + rowY(r), gridRight, rowH(r));
        }
    }

    int endCol = colOffset + w() / colWidth + 2;
    int colBottom = std::min(h(), gridBottom());

    // Subdivision lines first, so the row lines and column lines draw over them.
    if (divisions > 1) {
        fl_color(subdivLineColor);
        for (int i = colOffset; i < std::min(endCol, numCols); i++)
            for (int k = 1; k < divisions; k++) {
                // Round (not truncate) so these guide lines coincide with the
                // rounded note edges computed below.
                int x0 = x() + (i - colOffset) * colWidth
                             + (int)std::lround((double)k * colWidth / divisions);
                fl_line(x0, y(), x0, y() + colBottom);
            }
    }

    for (int i = 0; i < numRows + 1; i++) {
        fl_color(rowLineColor(i));
        fl_line(x(), y() + rowY(i), x() + gridRight, y() + rowY(i));
    }

    for (int i = colOffset; i <= std::min(endCol, numCols); i++) {
        int x0 = x() + (i - colOffset) * colWidth;
        fl_color(columnColor(i));
        fl_line(x0, y(), x0, y() + colBottom);
    }

    for (const Note& note : notes) {
        // Derive left and right edges by rounding each independently, then take
        // the width from their difference. Computing width from note.length on
        // its own truncates separately from x0, so with non-power-of-two
        // subdivisions (1/3, ...) a note's right edge and the next note's left
        // edge land on different pixels, leaving faint gaps between abutting
        // notes. Rounding both edges makes a note's right edge equal the next
        // note's left edge exactly — flush, with no gap and no overdraw.
        int xLeft  = (int)std::lround((note.beat - colOffset) * (double)colWidth);
        int xRight = (int)std::lround((note.beat + note.length - colOffset) * (double)colWidth);
        int x0     = x() + xLeft;
        int y0     = y() + rowY((int)note.row);
        int rh     = rowH((int)note.row);
        int width  = xRight - xLeft;
        if (x0 + width < x() || x0 > x() + w()) continue;
        drawNoteBlock(note, x0, y0, width, rh);
    }

    if (playhead)
        playhead->drawLine(x() - colOffset * colWidth, y(), rowY(numRows));

    fl_pop_clip();
}

int Grid::handle(int event)
{
    if (popup.visible())
        return 0;

    switch (event) {
        case FL_PUSH:
            // Refresh the hovered note on a right-click: while a context popup
            // was open this grid got no FL_MOVE events, so its hover state may
            // be stale (pointing at the previously-clicked note). Left-clicks
            // keep the existing state to preserve drag grab offsets.
            if (Fl::event_button() == FL_RIGHT_MOUSE || std::holds_alternative<StateIdle>(state))
                findNoteForCursor();
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                int idx = -1;
                if (auto* h = std::get_if<StateHoverMove>  (&state)) idx = h->noteIdx;
                else if (auto* h = std::get_if<StateHoverResize>(&state)) idx = h->noteIdx;
                if (idx >= 0)
                    openContextMenu(idx);
            } else if (auto* h = std::get_if<StateHoverMove>(&state)) {
                int   noteIdx = h->noteIdx;
                float grabX   = h->grabX;
                float grabY   = h->grabY;
                if (Fl::event_clicks() == 1) {
                    onNoteDoubleClick(noteIdx);
                    creationForbidden = true;  // prevent FL_RELEASE from calling toggleNote
                } else {
                    Point orig = {(int)notes[noteIdx].row, notes[noteIdx].beat};
                    onBeginDrag(noteIdx);
                    // Jump the cursor to the block's centre so it tracks the
                    // middle of the note during the drag instead of wherever it
                    // happened to be grabbed. Re-anchor grabX/grabY to the centre
                    // to match the warped cursor (only when the warp actually
                    // happened, so unwarped platforms keep the note in place
                    // rather than making it jump). The centre x is clamped to the
                    // visible area so a long note doesn't fling the cursor off
                    // the grid; grabX is then taken from the clamped position.
                    const Note& n  = notes[noteIdx];
                    int   row      = (int)n.row;
                    float centreY  = rowH(row) / 2.0f;
                    float leftEdge = (n.beat - colOffset) * colWidth;
                    float centreX  = std::clamp(leftEdge + n.length * colWidth / 2.0f,
                                                0.0f, (float)w());
                    if (warpPointerTo(window(), x() + (int)centreX,
                                                y() + rowY(row) + (int)centreY)) {
                        grabX = centreX - leftEdge;
                        grabY = centreY;
                    }
                    state = StateDragMove{noteIdx, grabX, grabY, orig, orig, false};
                }
            } else if (auto* h = std::get_if<StateHoverResize>(&state)) {
                int  noteIdx = h->noteIdx;
                Side side    = h->side;
                onBeginDrag(noteIdx);
                state = StateDragResize{noteIdx, side};
            } else {
                // Idle — check whether note creation is allowed at click position
                int   ex        = Fl::event_x() - x();
                int   row       = rowAtPixelY(Fl::event_y() - y());
                float fcol      = (float)ex / colWidth + colOffset;
                float col       = newNoteStart(fcol);
                float length    = newNoteLength();
                int   gridRight = std::min(w(), (numCols - colOffset) * colWidth);
                creationForbidden = ex >= gridRight;
                if (!creationForbidden) {
                    bool wouldRemove = std::any_of(notes.begin(), notes.end(),
                        [=, this](const Note& n) { return hitsNote(n, row, fcol); });
                    if (!wouldRemove) {
                        creationForbidden = std::any_of(notes.begin(), notes.end(),
                            [=](const Note& n) { return n.row == row && beatsOverlap(col, length, n.beat, n.length); });
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
                    notes[noteIdx].row = lastValid.row;
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

        case FL_LEAVE:
            state = StateIdle{};
            window()->cursor(FL_CURSOR_DEFAULT);
            return 0;

        case FL_MOVE:
            findNoteForCursor();
            return 0;

        case FL_KEYBOARD:
        case FL_SHORTCUT: {
            int key = Fl::event_key();
            if (key != FL_Delete && key != FL_BackSpace)
                return 0;
            int idx = -1;
            if (auto* h = std::get_if<StateHoverMove>  (&state)) idx = h->noteIdx;
            else if (auto* h = std::get_if<StateHoverResize>(&state)) idx = h->noteIdx;
            if (idx < 0)
                return 0;
            if (auto cb = makeDeleteCallback(idx)) {
                cb();
                state = StateIdle{};
                window()->cursor(FL_CURSOR_DEFAULT);
            }
            return 1;
        }

        default:
            return 0;
    }
}

void Grid::moving(StateDragMove& s)
{
    Note* note = &notes[s.noteIdx];
    float ex   = Fl::event_x() - x();
    note->beat  = (ex - s.grabX) / (float)colWidth + colOffset;
    if (snap > 0.0f) note->beat = std::round(note->beat / snap) * snap;
    // Clamp AFTER snapping so the note can't extend past the right edge. When
    // snapping is on, clamp the start DOWN to the last grid line that still
    // fits the note, otherwise a fractional-length note would land off-grid.
    if (note->beat < 0.0f) note->beat = 0.0f;
    if (note->beat + note->length > numCols) {
        float maxBeat = numCols - note->length;
        note->beat = snap > 0.0f ? std::floor(maxBeat / snap) * snap : maxBeat;
    }
    float ey   = Fl::event_y() - y();
    int newRow = std::clamp(rowAtPixelY(std::max(0, (int)(ey - s.grabY))), 0, numRows - 1);
    if (!isRowBlocked(newRow)) {
        note->row     = (float)newRow;
        s.overlapping = overlappingCell(s.noteIdx) >= 0;
        if (!s.overlapping) s.lastValid = {(int)note->row, note->beat};
    }
    if (s.overlapping) window()->cursor(forbiddenCursorImage(), 11, 11);
    else               window()->cursor(FL_CURSOR_HAND);
    redraw();
}

void Grid::resizing(StateDragResize& s)
{
    float minLength = 10.0f / colWidth;
    Note* note      = &notes[s.noteIdx];
    float ex        = Fl::event_x() - x();
    if (s.side == Side::Left) {
        float endCol  = note->beat + note->length;   // fixed (right) edge
        float newBeat = ex / (float)colWidth + colOffset;
        if (snap) newBeat = std::round(newBeat / snap) * snap;
        int   neighbour = overlappingCell(s.noteIdx);
        float min       = neighbour < 0 ? 0.0f : notes[neighbour].beat + notes[neighbour].length;
        if (newBeat < min) newBeat = min;
        // Enforce the minimum length WITHOUT dragging the moving edge off the
        // grid: if the snapped position is too close to the fixed edge, back
        // off to the nearest grid line that still leaves at least minLength.
        if (endCol - newBeat < minLength) {
            float limit = endCol - minLength;
            newBeat = snap ? std::floor(limit / snap) * snap : limit;
            if (newBeat < min) newBeat = min;
        }
        note->beat   = newBeat;
        note->length = endCol - newBeat;
    } else {
        float endCol = ex / (float)colWidth + colOffset;   // moving (right) edge
        if (snap) endCol = std::round(endCol / snap) * snap;
        int   neighbour = overlappingCell(s.noteIdx);
        float max       = neighbour < 0 ? (float)numCols : notes[neighbour].beat;
        if (endCol > max) endCol = max;
        if (endCol - note->beat < minLength) {
            float limit = note->beat + minLength;
            endCol = snap ? std::ceil(limit / snap) * snap : limit;
            if (endCol > max) endCol = max;
        }
        note->length = endCol - note->beat;
    }
    redraw();
}

void Grid::findNoteForCursor()
{
    const int resizeZone = 5;
    float ex  = Fl::event_x() - x();
    int   ey  = Fl::event_y() - y();
    int   row = rowAtPixelY(ey);

    int  resizeIdx  = -1;
    Side resizeSide = Side::Left;

    for (const auto [i, n] : std::views::enumerate(notes)) {
        if ((int)n.row != row) continue;
        float leftEdge  = (n.beat - colOffset) * colWidth;
        float rightEdge = (n.beat + n.length - colOffset) * colWidth;

        if (leftEdge - ex <= resizeZone && ex - leftEdge <= resizeZone) {
            resizeIdx = i; resizeSide = Side::Left;
        } else if (rightEdge - ex <= resizeZone && ex - rightEdge <= resizeZone) {
            resizeIdx = i; resizeSide = Side::Right;
        } else if (ex >= leftEdge && ex <= rightEdge) {
            state = StateHoverMove{(int)i, ex - leftEdge, (float)(ey - rowY((int)n.row))};
            window()->cursor(contextMenuCursorImage(), 0, 0);
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

float Grid::newNoteStart(float fcol) const
{
    float beat = snap > 0.0f ? std::floor(fcol / snap) * snap : fcol;
    return std::clamp(beat, 0.0f, (float)numCols - newNoteLength());
}

void Grid::toggleNote()
{
    int   ex     = Fl::event_x() - x();
    int   ey     = Fl::event_y() - y();
    int   row    = rowAtPixelY(ey);
    float fcol   = (float)ex / colWidth + colOffset;
    float col    = newNoteStart(fcol);
    float length = newNoteLength();

    int size = notes.size();
    notes.erase(std::remove_if(notes.begin(), notes.end(),
        [=, this](const Note& n) { return hitsNote(n, row, fcol); }), notes.end());
    if ((int)notes.size() == size) {
        bool clear = std::none_of(notes.begin(), notes.end(),
            [=](const Note& n) { return n.row == row && beatsOverlap(col, length, n.beat, n.length); });
        if (clear)
            notes.push_back({0, row, col, length});
    }
    redraw();
}

int Grid::overlappingCell(int noteIdx) const
{
    Note a = notes[noteIdx];
    for (const auto [i, b] : std::views::enumerate(notes)) {
        if (i == noteIdx || b.row != a.row) continue;
        if (beatsOverlap(a.beat, a.length, b.beat, b.length)) return i;
    }
    return -1;
}

void Grid::openContextMenu(int idx)
{
    popup.open(idx, &notes, this, makeDeleteCallback(idx), makeVelocityCallback(idx));
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

void Grid::drawNoteBlock(const Note& note, int x0, int y0, int width, int rh)
{
    const Fl_Color fill = velocityFill(note.velocity);
    const Fl_Color bar  = velocityAccent(note.velocity);
    fl_rectf(x0, y0 + 1, width, rh - 1, fill);
    const int barWidth = 5;
    fl_color(bar);
    fl_line_style(FL_SOLID, barWidth);
    fl_line(x0 + barWidth / 2, y0 + 1, x0 + barWidth / 2, y0 + rh - 1);
    fl_line_style(0);
}
