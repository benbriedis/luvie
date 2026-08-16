// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "drumGrid.hpp"
#include "grid.hpp"
#include "playhead.hpp"
#include "editor.hpp"
#include "cursors.hpp"
#include "noteColor.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cmath>

static constexpr Fl_Color kRowLine = 0xEE888800;
static constexpr Fl_Color kBarCol  = 0x00660000;
static constexpr Fl_Color kBeatCol = 0x00EE0000;

DrumGrid::DrumGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
    : Fl_Box(0, 0, numCols * colWidth, numRows * rowHeight),
      snap(snap), popup(popup),
      numRows(numRows), numCols(numCols), rowHeight(rowHeight), colWidth(colWidth)
{
    box(FL_NO_BOX);
    // Margin before beat 0: full dot radius plus a little breathing room, so a
    // note on beat 1 shows its whole circle and is easy to click.
    padX = std::max(2, rowHeight / 3) + 3;
}

DrumGrid::~DrumGrid()
{
    swapObserver(pattern, nullptr, this);
}

void DrumGrid::setPattern(ObservablePattern* tl, int patId)
{
    swapObserver(pattern, tl, this);
    patternId = patId;
    rebuildNotes();
    redraw();
}

void DrumGrid::rebuildNotes()
{
    notes.clear();
    if (!pattern || patternId < 0) { selection.clear(); return; }
    auto all = pattern->buildDrumPatternNotes(patternId);
    for (const auto& n : all) {
        int vr = rowOffset + numRows - 1 - n.note;
        if (vr >= 0 && vr < numRows)
            notes.push_back(n);
    }
    // Drop ids the pattern no longer has. Checked against the whole pattern,
    // not the visible rows, so scrolling does not shrink the selection.
    if (!selection.empty()) {
        std::unordered_set<int> live;
        for (const auto& n : all) live.insert(n.id);
        selection.retain(live);
    }
}

void DrumGrid::onTimelineChanged()
{
    if (!isActiveDrag())
        rebuildNotes();
    redraw();
}

void DrumGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    rebuildNotes();
    redraw();
}

// Returns the index into notes[] of the note whose dot center is closest to
// the cursor, within a reasonable hit radius. Returns -1 if none.
int DrumGrid::findNoteAtCursor() const
{
    int ex = Fl::event_x() - x();
    int ey = Fl::event_y() - y();
    int vr = ey / rowHeight;
    int midiNote = rowOffset + numRows - 1 - vr;

    const int dotR   = std::max(2, rowHeight / 6);
    const int hitR   = dotR + 4;   // slightly larger than drawn dot for easier clicking

    int   bestIdx  = -1;
    float bestDist = (float)hitR;

    for (int i = 0; i < (int)notes.size(); i++) {
        if (notes[i].note != midiNote) continue;
        float dotX = padX + (notes[i].beat - colOffset) * colWidth;
        float dist = std::abs((float)ex - dotX);
        if (dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

void DrumGrid::draw()
{
    Fl_Box::draw();
    fl_push_clip(x(), y(), w() + 1, h() + 1);

    fl_color(bgColor);
    fl_rectf(x(), y(), w(), h());

    int gridRight = std::min(w(), padX + (numCols - colOffset) * colWidth);
    int endCol    = colOffset + w() / colWidth + 2;

    // Subdivision lines first, so the row lines and column lines draw over them.
    if (divisions > 1) {
        fl_color(subdivLineColor);
        for (int i = colOffset; i < std::min(endCol, numCols); i++)
            for (int k = 1; k < divisions; k++) {
                int x0 = x() + padX + (i - colOffset) * colWidth + k * colWidth / divisions;
                fl_line(x0, y(), x0, y() + h());
            }
    }

    // Horizontal row lines — uniform (no pitch-group colouring)
    for (int i = 0; i <= numRows; i++) {
        fl_color(kRowLine);
        fl_line(x(), y() + i * rowHeight, x() + gridRight, y() + i * rowHeight);
    }

    // Vertical column lines
    for (int i = colOffset; i <= std::min(endCol, numCols); i++) {
        int x0 = x() + padX + (i - colOffset) * colWidth;
        // Bar lines come from the pattern's own time signature, as in HarmonyGrid.
        bool isBar = (i % 4 == 0);
        if (pattern)
            if (const Pattern* p = pattern->song()->patternById(patternId))
                isBar = p->timeSigTop > 0 && (i % p->timeSigTop == 0);
        fl_color(isBar ? kBarCol : kBeatCol);
        fl_line(x0, y(), x0, y() + h());
    }

    // Draw drum notes as filled circles at the beat position (not cell center)
    const int dotR = std::max(2, rowHeight / 3);
    for (int i = 0; i < (int)notes.size(); i++) {
        const auto& n = notes[i];
        int vr = rowOffset + numRows - 1 - n.note;
        if (vr < 0 || vr >= numRows) continue;
        int dotX = x() + padX + (int)((n.beat - colOffset) * colWidth);
        int dotY = y() + vr * rowHeight + rowHeight / 2;
        if (dotX + dotR < x() || dotX - dotR > x() + w()) continue;
        fl_color(velocityFill(n.velocity));
        fl_pie(dotX - dotR, dotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
        fl_color(velocityAccent(n.velocity));
        fl_arc(dotX - dotR, dotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
        if (selection.contains(n.id)) {
            const int r = dotR + 2;
            fl_color(selectionColor);
            fl_line_style(FL_SOLID, 2);
            fl_arc(dotX - r, dotY - r, 2 * r, 2 * r, 0, 360);
            fl_line_style(0);
        }
    }

    // Rubber band, over the dots.
    if (selection.active) {
        const int bx = x() + selection.bandLeft(), bw = selection.bandRight()  - selection.bandLeft();
        const int by = y() + selection.bandTop(),  bh = selection.bandBottom() - selection.bandTop();
        if (bw > 0 || bh > 0) {
            fl_color(fl_color_average(bandColor, bgColor, 0.18f));
            fl_rectf(bx, by, bw, bh);
            fl_color(bandColor);
            fl_line_style(FL_DASH, 1);
            fl_rect(bx, by, bw, bh);
            fl_line_style(0);
        }
    }

    if (playhead)
        playhead->drawLine(x() + padX - colOffset * colWidth, y(), numRows * rowHeight);

    fl_pop_clip();
}

int DrumGrid::handle(int evt)
{
    if (popup.visible()) return 0;

    switch (evt) {

    case FL_PUSH: {
        int idx = findNoteAtCursor();

        const int mods = Fl::event_state();
        if (Fl::event_button() == FL_LEFT_MOUSE && (mods & FL_SHIFT)) {
            selection.beginBand(Fl::event_x() - x(), Fl::event_y() - y());
            state = DrumStateBand{(mods & FL_COMMAND) != 0};
            creationForbidden = true;
            redraw();
            return 1;
        }
        if (Fl::event_button() == FL_LEFT_MOUSE && (mods & FL_COMMAND)) {
            if (idx >= 0) {
                selection.toggle(notes[idx].id);
                redraw();
            }
            state = DrumStateIdle{};
            creationForbidden = true;   // never create on a ctrl-click
            return 1;
        }

        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (idx >= 0) {
                // Open context popup for this drum note
                const auto& n = notes[idx];
                int vr   = rowOffset + numRows - 1 - n.note;
                int dotX = x() + padX + (int)((n.beat - colOffset) * colWidth);
                int dotY = y() + vr * rowHeight + rowHeight / 2;
                int id   = n.id;
                popup.openForDot(dotX, dotY, this, rowHeight, n.velocity,
                    [this, id]() { if (pattern) pattern->removeDrumNote(id); },
                    [this, id](float v) { if (pattern) pattern->setDrumNoteVelocity(id, v); });
            }
            return 1;
        }

        // Left mouse
        if (idx >= 0 && selection.contains(notes[idx].id)) {
            // Grabbing a member of the selection drags all of it. No pointer
            // warp: snapping the cursor to one dot is disorienting when many
            // are moving.
            beginGroupDrag(idx, Fl::event_x(), Fl::event_y());
            if (window()) window()->cursor(FL_CURSOR_HAND);
        } else if (idx >= 0) {
            if (!selection.empty()) { selection.clear(); redraw(); }
            // Jump the cursor to the note's centre so it tracks the middle of
            // the note during the drag. grabX/grabY hold the window position the
            // drag is anchored to; use the warped centre when the warp actually
            // happened, otherwise keep the grabbed position (unwarped platforms).
            int vr         = rowOffset + numRows - 1 - notes[idx].note;
            int centreWinX = x() + padX + (int)((notes[idx].beat - colOffset) * colWidth);
            int centreWinY = y() + vr * rowHeight + rowHeight / 2;
            int grabX      = Fl::event_x();
            int grabY      = Fl::event_y();
            if (warpPointerTo(window(), centreWinX, centreWinY)) {
                grabX = centreWinX;
                grabY = centreWinY;
            }
            // Start drag on this note
            state = DrumStateDrag{
                idx,
                grabX, grabY,
                notes[idx].beat, notes[idx].note,
                false
            };
            if (window()) window()->cursor(FL_CURSOR_HAND);
        } else if (!selection.empty()) {
            // A plain click on empty space with a selection active clears it and
            // creates nothing, so a selection can be dismissed without editing.
            // The release still arrives, so it has to be told to stay its hand.
            selection.clear();
            state = DrumStateIdle{};
            creationForbidden = true;
            redraw();
            return 1;
        } else {
            state = DrumStateIdle{};
            creationForbidden = false;
        }
        return 1;
    }

    case FL_DRAG: {
        if (std::holds_alternative<DrumStateBand>(state)) {
            selection.updateBand(Fl::event_x() - x(), Fl::event_y() - y());
            redraw();
            return 1;
        }
        if (auto* g = std::get_if<DrumStateDragGroup>(&state)) {
            movingGroup(*g);
            return 1;
        }
        if (auto* d = std::get_if<DrumStateDrag>(&state)) {
            int   ex       = Fl::event_x() - x();
            int   ey       = Fl::event_y() - y();

            // Horizontal: update beat
            float newBeat = d->origBeat + (float)(ex - (d->grabX - x())) / colWidth;
            newBeat = std::max(0.0f, std::min((float)numCols, newBeat));
            if (snap > 0.0f) newBeat = std::round(newBeat / snap) * snap;
            // Keep the note off the final vertical line (end of pattern):
            // pull it back to the last valid position, matching createNote().
            if (newBeat >= (float)numCols)
                newBeat = snap > 0.0f ? (float)numCols - snap
                                      : std::nextafter((float)numCols, 0.0f);

            // Vertical: update MIDI note
            int rowDelta = (ey - (d->grabY - y())) / rowHeight;
            int newMidi  = std::clamp(d->origNote - rowDelta, 0, 127);

            notes[d->noteIdx].beat = newBeat;
            notes[d->noteIdx].note = newMidi;

            if (newBeat != d->origBeat || newMidi != d->origNote)
                d->moved = true;

            redraw();
        }
        return 1;
    }

    case FL_RELEASE: {
        if (auto* b = std::get_if<DrumStateBand>(&state)) {
            bool additive = b->additive;
            state = DrumStateIdle{};
            applyBand(additive);
            creationForbidden = false;
            if (window()) window()->cursor(FL_CURSOR_DEFAULT);
            return 1;
        }
        if (auto* g = std::get_if<DrumStateDragGroup>(&state)) {
            DrumStateDragGroup drag = *g;
            state = DrumStateIdle{};   // clear BEFORE the model ops so rebuild runs
            if (!drag.blocked && (drag.dBeat != 0.0f || drag.dNote != 0))
                commitGroupMove(drag.dBeat, drag.dNote);
            else
                redraw();
            groupOrig.clear();
            if (window()) window()->cursor(FL_CURSOR_DEFAULT);
            return 1;
        }
        if (auto* d = std::get_if<DrumStateDrag>(&state)) {
            if (d->moved && pattern) {
                // Capture before clearing state
                int   id       = notes[d->noteIdx].id;
                float beat     = notes[d->noteIdx].beat;
                int   midiNote = notes[d->noteIdx].note;
                state = DrumStateIdle{};  // clear BEFORE pattern ops so rebuild runs
                pattern->moveDrumNote(id, midiNote, beat);
            } else if (!d->moved && pattern) {
                // Pure click on existing note → remove it
                int id = notes[d->noteIdx].id;
                state = DrumStateIdle{};  // clear BEFORE so onTimelineChanged rebuilds
                pattern->removeDrumNote(id);
            } else {
                state = DrumStateIdle{};
            }
        } else {
            state = DrumStateIdle{};
            // Click on empty space → create note
            if (Fl::event_button() == FL_LEFT_MOUSE && !creationForbidden)
                createNote();
            creationForbidden = false;
        }
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        return 1;
    }

    case FL_ENTER:
        return 1;

    case FL_MOVE: {
        int idx = findNoteAtCursor();
        if (idx >= 0) {
            state = DrumStateHover{idx};
            if (window()) window()->cursor(contextMenuCursorImage(), 0, 0);
        } else {
            state = DrumStateIdle{};
            if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        }
        return 0;
    }

    case FL_LEAVE:
        state = DrumStateIdle{};
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        return 0;

    case FL_KEYBOARD:
    case FL_SHORTCUT: {
        int key = Fl::event_key();
        // Unfocused widget: FLTK broadcasts the shortcut, so the cursor decides
        // which grid it was meant for — the rule the hover-delete needs. The
        // whole-grid commands (Ctrl-A, Delete with a selection) are window-level
        // in AppWindow, so they don't care where the cursor is.
        if (!Fl::event_inside(this))
            return 0;
        if (key != FL_Delete && key != FL_BackSpace)
            return 0;
        auto* h = std::get_if<DrumStateHover>(&state);
        if (!h || !pattern)
            return 0;
        int id = notes[h->noteIdx].id;
        state = DrumStateIdle{};
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        pattern->removeDrumNote(id);
        return 1;
    }

    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Multi-selection
//
// Drum notes are point events keyed by MIDI note rather than ranges on a row,
// so none of Grid's range geometry carries over — hence the parallel
// implementation. The grid is drawn highest note first, so moving DOWN the
// screen by dNote rows SUBTRACTS dNote from the MIDI note.
// ---------------------------------------------------------------------------

void DrumGrid::selectAllNotes()
{
    selection.clear();
    if (!pattern || patternId < 0) return;
    for (const DrumNote& n : pattern->buildDrumPatternNotes(patternId))
        selection.add(n.id);
}

// Delete arrives from AppWindow, which knows nothing of hover, so the state has
// to be dropped here: it may name a note that no longer exists.
void DrumGrid::deleteSelectedItems()
{
    deleteSelection();
    state = DrumStateIdle{};
    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
}

void DrumGrid::deleteSelection()
{
    if (!pattern || selection.empty()) return;
    std::vector<int> doomed(selection.ids().begin(), selection.ids().end());
    selection.clear();
    ObservableSong::Batch batch(pattern->song());
    for (int id : doomed) pattern->removeDrumNote(id);
}

void DrumGrid::groupDragLimits(float& minDBeat, float& maxDBeat,
                               int& minDNote, int& maxDNote) const
{
    minDBeat = maxDBeat = 0.0f;
    minDNote = maxDNote = 0;
    if (!pattern || patternId < 0) return;

    bool first = true;
    for (const DrumNote& n : pattern->buildDrumPatternNotes(patternId)) {
        if (!selection.contains(n.id)) continue;
        // A drum note may sit anywhere in [0, numCols) — never on the closing
        // line, which createNote() also refuses.
        float bLo = -n.beat;
        float bHi = (float)numCols - n.beat - (snap > 0.0f ? snap : 0.0f);
        int   rLo = n.note - 127;
        int   rHi = n.note;
        if (first) { minDBeat = bLo; maxDBeat = bHi; minDNote = rLo; maxDNote = rHi; first = false; }
        else {
            minDBeat = std::max(minDBeat, bLo);
            maxDBeat = std::min(maxDBeat, bHi);
            minDNote = std::max(minDNote, rLo);
            maxDNote = std::min(maxDNote, rHi);
        }
    }
}

// Two drum hits on the same note at the same beat would be indistinguishable,
// so a move onto an unselected hit is refused. Selected hits all shift together
// and so cannot newly collide with each other.
bool DrumGrid::groupMoveBlocked(float dBeat, int dNote) const
{
    if (!pattern || patternId < 0) return false;
    const float eps = 1e-4f;
    auto all = pattern->buildDrumPatternNotes(patternId);
    for (const DrumNote& n : all) {
        if (!selection.contains(n.id)) continue;
        float beat = n.beat + dBeat;
        int   note = n.note - dNote;
        for (const DrumNote& other : all) {
            if (selection.contains(other.id)) continue;
            if (other.note != note) continue;
            if (std::abs(other.beat - beat) < eps) return true;
        }
    }
    return false;
}

void DrumGrid::commitGroupMove(float dBeat, int dNote)
{
    if (!pattern || patternId < 0) return;
    std::vector<DrumNote> sel;
    for (const DrumNote& n : pattern->buildDrumPatternNotes(patternId))
        if (selection.contains(n.id)) sel.push_back(n);

    ObservableSong::Batch batch(pattern->song());
    for (const DrumNote& n : sel)
        pattern->moveDrumNote(n.id, n.note - dNote, n.beat + dBeat);
}

void DrumGrid::beginGroupDrag(int idx, int grabX, int grabY)
{
    groupOrig.clear();
    for (int i = 0; i < (int)notes.size(); ++i)
        if (selection.contains(notes[i].id))
            groupOrig.push_back({i, notes[i].beat, notes[i].note});
    state = DrumStateDragGroup{grabX, grabY, notes[idx].beat, notes[idx].note, 0.0f, 0, false};
}

void DrumGrid::movingGroup(DrumStateDragGroup& d)
{
    int ex = Fl::event_x() - x();
    int ey = Fl::event_y() - y();

    float beat = d.origBeat + (float)(ex - (d.grabX - x())) / colWidth;
    if (snap > 0.0f) beat = std::round(beat / snap) * snap;
    float dBeat = beat - d.origBeat;

    int rowDelta = (ey - (d.grabY - y())) / rowHeight;
    int dNote    = rowDelta;   // screen-down is a lower MIDI note; see header comment

    float minDB, maxDB; int minDN, maxDN;
    groupDragLimits(minDB, maxDB, minDN, maxDN);
    dBeat = std::clamp(dBeat, minDB, maxDB);
    dNote = std::clamp(dNote, minDN, maxDN);

    d.dBeat   = dBeat;
    d.dNote   = dNote;
    d.blocked = groupMoveBlocked(dBeat, dNote);

    for (const auto& g : groupOrig) {
        notes[g.idx].beat = g.beat + dBeat;
        notes[g.idx].note = g.note - dNote;
    }

    if (window()) {
        if (d.blocked) window()->cursor(forbiddenCursorImage(), 11, 11);
        else           window()->cursor(FL_CURSOR_HAND);
    }
    redraw();
}

void DrumGrid::applyBand(bool additive)
{
    if (!additive) selection.clear();
    for (const DrumNote& n : notes) {
        int vr = rowOffset + numRows - 1 - n.note;
        if (vr < 0 || vr >= numRows) continue;
        int dotX = padX + (int)((n.beat - colOffset) * colWidth);
        int dotY = vr * rowHeight + rowHeight / 2;
        if (selection.bandContainsPoint(dotX, dotY)) selection.add(n.id);
    }
    selection.endBand();
    redraw();
}

void DrumGrid::createNote()
{
    int   ex       = Fl::event_x() - x();
    int   ey       = Fl::event_y() - y();
    int   gridRight = std::min(w(), padX + (numCols - colOffset) * colWidth);
    if (ex >= gridRight) return;

    int   vr       = ey / rowHeight;
    float beat     = (float)(ex - padX) / colWidth + colOffset;
    if (snap > 0.0f) beat = std::round(beat / snap) * snap;
    // A click in the left margin (before beat 0) shouldn't create a note.
    if (beat < 0.0f) return;
    // Don't place a note on the final vertical line: it's the end of the
    // pattern and visually identical to placing one at the start (beat 0).
    if (beat >= (float)numCols) return;
    int   midiNote = rowOffset + numRows - 1 - vr;
    if (midiNote < 0 || midiNote > 127) return;
    if (!pattern || patternId < 0) return;

    pattern->addDrumNote(patternId, midiNote, beat);
}

void DrumGrid::removeNote(int idx)
{
    if (!pattern || idx < 0 || idx >= (int)notes.size()) return;
    pattern->removeDrumNote(notes[idx].id);
}
