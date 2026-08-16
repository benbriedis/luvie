// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "grid.hpp"
#include "playhead.hpp"
#include "editor.hpp"
#include "noteColor.hpp"
#include <FL/Fl.H>
#include "FL/Enumerations.H"
#include "noteContextPopup.hpp"
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

    // Vertical lines skip the rows that opt out (instrument header rows in the
    // song editor), so draw them as spans of consecutive rows that want them
    // rather than as one full-height line.
    vector<std::pair<int, int>> colSpans;
    {
        auto addSpan = [&](int top, int bottom) {
            bottom = std::min(bottom, colBottom);
            if (bottom > top) colSpans.emplace_back(top, bottom);
        };
        int runStart = -1;
        for (int r = 0; r < numRows; r++) {
            if (rowHidesColumnLines(r)) {
                if (runStart >= 0) addSpan(runStart, rowY(r));
                runStart = -1;
            } else if (runStart < 0) {
                runStart = rowY(r);
            }
        }
        int rowsBottom = rowY(numRows);
        if (runStart >= 0) addSpan(runStart, rowsBottom);
        // Any empty area below the last row still gets full-height lines.
        addSpan(rowsBottom, colBottom);
    }

    // Subdivision lines first, so the row lines and column lines draw over them.
    if (divisions > 1) {
        fl_color(subdivLineColor);
        for (int i = colOffset; i < std::min(endCol, numCols); i++)
            for (int k = 1; k < divisions; k++) {
                // Round (not truncate) so these guide lines coincide with the
                // rounded note edges computed below.
                int x0 = x() + (i - colOffset) * colWidth
                             + (int)std::lround((double)k * colWidth / divisions);
                for (const auto& [top, bottom] : colSpans)
                    fl_line(x0, y() + top, x0, y() + bottom);
            }
    }

    for (int i = 0; i < numRows + 1; i++) {
        fl_color(rowLineColor(i));
        fl_line(x(), y() + rowY(i), x() + gridRight, y() + rowY(i));
    }

    for (int i = colOffset; i <= std::min(endCol, numCols); i++) {
        int x0 = x() + (i - colOffset) * colWidth;
        fl_color(columnColor(i));
        for (const auto& [top, bottom] : colSpans)
            fl_line(x0, y() + top, x0, y() + bottom);
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
        if (selection.contains(note.id))
            drawSelectionOutline(x0, y0, width, rh);
    }

    drawBand();

    if (playhead)
        playhead->drawLine(x() - colOffset * colWidth, y(), rowY(numRows));

    fl_pop_clip();
}

int Grid::handle(int event)
{
    if (popup.visible())
        return 0;

    switch (event) {
        case FL_PUSH: {
            // Refresh the hovered note on a right-click: while a context popup
            // was open this grid got no FL_MOVE events, so its hover state may
            // be stale (pointing at the previously-clicked note). Left-clicks
            // keep the existing state to preserve drag grab offsets.
            if (Fl::event_button() == FL_RIGHT_MOUSE || std::holds_alternative<StateIdle>(state))
                findNoteForCursor();

            const int mods = Fl::event_state();
            if (Fl::event_button() == FL_LEFT_MOUSE && (mods & FL_SHIFT)) {
                // Shift-drag always sweeps a band, even when the press lands on
                // a note — otherwise notes could not be band-selected from the
                // middle of a dense pattern.
                selection.beginBand(Fl::event_x() - x(), Fl::event_y() - y());
                state = StateBandSelect{(mods & FL_COMMAND) != 0};
                creationForbidden = true;
                redraw();
                return 1;
            }
            if (Fl::event_button() == FL_LEFT_MOUSE && (mods & FL_COMMAND)) {
                // Ctrl-click toggles one item in or out of the selection.
                int idx = -1;
                if (auto* h = std::get_if<StateHoverMove>  (&state)) idx = h->noteIdx;
                else if (auto* h = std::get_if<StateHoverResize>(&state)) idx = h->noteIdx;
                if (idx >= 0) {
                    selection.toggle(notes[idx].id);
                    state = StateIdle{};
                    redraw();
                }
                creationForbidden = true;   // never create/delete on a ctrl-click
                return 1;
            }

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
                } else if (selection.contains(notes[noteIdx].id)) {
                    // Grabbing a member of the selection drags the whole of it.
                    beginGroupDrag(noteIdx, grabX, grabY);
                } else {
                    // Grabbing anything else drops the selection and moves that
                    // one item, exactly as before.
                    if (!selection.empty()) { selection.clear(); redraw(); }
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
            } else if (!selection.empty()) {
                // A plain click on empty space with a selection active just
                // clears it. Creating as well would make it impossible to
                // dismiss a selection without also editing something.
                selection.clear();
                creationForbidden = true;
                redraw();
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
        }

        case FL_DRAG:
            if (auto* s = std::get_if<StateDragMove>  (&state)) moving(*s);
            else if (auto* s = std::get_if<StateDragResize>(&state)) resizing(*s);
            else if (auto* s = std::get_if<StateDragGroup> (&state)) movingGroup(*s);
            else if (std::holds_alternative<StateBandSelect>(state)) {
                selection.updateBand(Fl::event_x() - x(), Fl::event_y() - y());
                redraw();
            }
            return 1;

        case FL_RELEASE:
            if (auto* s = std::get_if<StateBandSelect>(&state)) {
                bool additive = s->additive;
                state = StateIdle{};
                applyBand(additive);
                creationForbidden = false;
                window()->cursor(FL_CURSOR_DEFAULT);
                return 1;
            }
            if (auto* s = std::get_if<StateDragGroup>(&state)) {
                StateDragGroup drag = *s;
                state = StateIdle{};   // clear BEFORE commit so isActiveDrag() is false
                if (!drag.blocked && (drag.dBeat != 0.0f || drag.dRow != 0))
                    onCommitGroupMove(drag.dBeat, drag.dRow);
                else
                    redraw();          // snap the preview back
                // Whether the move committed or not, the model is now the truth
                // for the items previewed outside `notes`.
                previewGroupExtras(0.0f);
                groupOrig.clear();
                window()->cursor(FL_CURSOR_DEFAULT);
                return 1;
            }
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
            // These are unfocused grids, so FLTK broadcasts the shortcut to all
            // of them; the cursor decides which one it was meant for. Only the
            // hover-delete below needs that rule, and it is all that is left
            // here: the commands that act on the grid as a whole — Ctrl-A, and
            // Delete with a selection — are handled by AppWindow, so the cursor
            // can be anywhere in the window.
            if (!Fl::event_inside(this))
                return 0;
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

// ---------------------------------------------------------------------------
// Multi-selection
// ---------------------------------------------------------------------------

// Default implementations work off the visible notes. The subclasses that can
// see the whole model override them so a selection reaches past the viewport.
std::unordered_set<int> Grid::liveItemIds() const
{
    std::unordered_set<int> ids;
    for (const Note& n : notes) ids.insert(n.id);
    return ids;
}

void Grid::selectAll()       { selection.clear(); for (const Note& n : notes) selection.add(n.id); }
void Grid::deleteSelection() {}

// Delete arrives from AppWindow, which knows nothing of hover, so the state has
// to be dropped here: it may name a note that no longer exists.
void Grid::deleteSelectedItems()
{
    deleteSelection();
    state = StateIdle{};
    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
}

void Grid::groupDragLimits(float& minDBeat, float& maxDBeat, int& minDRow, int& maxDRow) const
{
    minDBeat = maxDBeat = 0.0f;
    minDRow  = maxDRow  = 0;
    bool first = true;
    for (const Note& n : notes) {
        if (!selection.contains(n.id)) continue;
        float lo = -n.beat;                            // furthest left this note may go
        float hi = (float)numCols - (n.beat + n.length);
        int   rlo = -(int)n.row;
        int   rhi = numRows - 1 - (int)n.row;
        if (first) { minDBeat = lo; maxDBeat = hi; minDRow = rlo; maxDRow = rhi; first = false; }
        else {
            minDBeat = std::max(minDBeat, lo);
            maxDBeat = std::min(maxDBeat, hi);
            minDRow  = std::max(minDRow, rlo);
            maxDRow  = std::min(maxDRow, rhi);
        }
    }
}

bool Grid::groupMoveBlocked(float dBeat, int dRow) const
{
    for (const auto& g : groupOrig) {
        const Note& n = notes[g.idx];
        float beat = g.beat + dBeat;
        int   row  = g.row  + dRow;
        if (isRowBlocked(row)) return true;
        for (const Note& other : notes) {
            if (selection.contains(other.id)) continue;   // moves with us
            if ((int)other.row != row) continue;
            if (beatsOverlap(beat, n.length, other.beat, other.length)) return true;
        }
    }
    return false;
}

void Grid::onCommitGroupMove(float, int) {}

void Grid::beginGroupDrag(int noteIdx, float grabX, float grabY)
{
    onBeginDrag(noteIdx);
    beginGroupDrag(Point{(int)notes[noteIdx].row, notes[noteIdx].beat}, grabX, grabY);
}

void Grid::beginGroupDrag(Point original, float grabX, float grabY)
{
    // No pointer warp here. Snapping the cursor to one block's centre is a
    // helpful cue when that block is the only thing moving, and disorienting
    // when it is one of fifty.
    groupOrig.clear();
    for (int i = 0; i < (int)notes.size(); ++i)
        if (selection.contains(notes[i].id))
            groupOrig.push_back({i, notes[i].beat, (int)notes[i].row});

    // Resolve the travel limits now, from the untouched starting positions.
    // They must not be recomputed while the drag runs: some of the inputs are
    // the preview positions themselves, so the bound would slide along with the
    // selection instead of holding it.
    float minDB, maxDB; int minDR, maxDR;
    groupDragLimits(minDB, maxDB, minDR, maxDR);
    includeZero(minDB, maxDB);
    includeZero(minDR, maxDR);

    state = StateDragGroup{grabX, grabY, original, minDB, maxDB, minDR, maxDR,
                           0.0f, 0, false};
}

void Grid::movingGroup(StateDragGroup& s)
{
    // The primary follows the cursor under the ordinary snapping rules; the
    // delta it lands on is what the rest of the selection inherits.
    float ex   = Fl::event_x() - x();
    float beat = (ex - s.grabX) / (float)colWidth + colOffset;
    if (snap > 0.0f) beat = std::round(beat / snap) * snap;
    float dBeat = beat - s.original.col;

    int dRow = 0;
    if (allowsVerticalDrag()) {
        float ey = Fl::event_y() - y();
        int newRow = rowAtPixelY(std::max(0, (int)(ey - s.grabY)));
        dRow = newRow - s.original.row;
    }

    // Clamp the delta so no member of the selection leaves the grid, rather
    // than clamping each note as it goes — that would squash the shape. The
    // limits were fixed when the drag began.
    dBeat = std::clamp(dBeat, s.minDBeat, s.maxDBeat);
    dRow  = std::clamp(dRow,  s.minDRow,  s.maxDRow);
    if (snap > 0.0f) {
        // Re-snap after clamping: the limit itself is rarely on a grid line.
        float snapped = std::round(dBeat / snap) * snap;
        if (snapped >= s.minDBeat && snapped <= s.maxDBeat) dBeat = snapped;
    }

    s.dBeat   = dBeat;
    s.dRow    = dRow;
    s.blocked = groupMoveBlocked(dBeat, dRow);

    // Preview from the originals, so repeated moves cannot drift.
    for (const auto& g : groupOrig) {
        notes[g.idx].beat = g.beat + dBeat;
        notes[g.idx].row  = g.row  + dRow;
    }
    previewGroupExtras(dBeat);

    if (s.blocked) window()->cursor(forbiddenCursorImage(), 11, 11);
    else           window()->cursor(FL_CURSOR_HAND);
    redraw();
}

void Grid::applyBand(bool additive)
{
    if (!additive) selection.clear();

    const int left = selection.bandLeft(), right = selection.bandRight();
    for (const Note& n : notes) {
        int row = (int)n.row;
        if (row < 0 || row >= numRows) continue;
        if (isRowBlocked(row)) continue;
        if (!selection.bandCoversRow(rowY(row), rowH(row))) continue;
        int nLeft  = (int)std::lround((n.beat - colOffset) * (double)colWidth);
        int nRight = (int)std::lround((n.beat + n.length - colOffset) * (double)colWidth);
        if (nRight < left || nLeft > right) continue;   // horizontal: any overlap
        selection.add(n.id);
    }
    addBandHitExtras();
    selection.endBand();
    redraw();
}

void Grid::drawBand() const
{
    if (!selection.active) return;
    const int bx = x() + selection.bandLeft(),  bw = selection.bandRight()  - selection.bandLeft();
    const int by = y() + selection.bandTop(),   bh = selection.bandBottom() - selection.bandTop();
    if (bw <= 0 && bh <= 0) return;
    fl_color(fl_color_average(bandColor, bgColor, 0.18f));
    fl_rectf(bx, by, bw, bh);
    fl_color(bandColor);
    fl_line_style(FL_DASH, 1);
    fl_rect(bx, by, bw, bh);
    fl_line_style(0);
}

void Grid::drawSelectionOutline(int x0, int y0, int width, int rh) const
{
    fl_color(selectionColor);
    fl_line_style(FL_SOLID, 2);
    fl_rect(x0 + 1, y0 + 2, std::max(2, width - 2), rh - 3);
    fl_line_style(0);
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

    for (int i = 0; i < (int)notes.size(); ++i) {
        const Note& n = notes[i];
        if ((int)n.row != row) continue;
        float leftEdge  = (n.beat - colOffset) * colWidth;
        float rightEdge = (n.beat + n.length - colOffset) * colWidth;

        if (leftEdge - ex <= resizeZone && ex - leftEdge <= resizeZone) {
            resizeIdx = i; resizeSide = Side::Left;
        } else if (rightEdge - ex <= resizeZone && ex - rightEdge <= resizeZone) {
            resizeIdx = i; resizeSide = Side::Right;
        } else if (ex >= leftEdge && ex <= rightEdge) {
            state = StateHoverMove{i, ex - leftEdge, (float)(ey - rowY((int)n.row))};
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
    for (int i = 0; i < (int)notes.size(); ++i) {
        const Note& b = notes[i];
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
    // Drop ids the model has since lost. Checked against the model rather than
    // `notes`, which holds only the visible rows — scrolling must not silently
    // shrink the selection.
    if (!selection.empty())
        selection.retain(liveItemIds());

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
