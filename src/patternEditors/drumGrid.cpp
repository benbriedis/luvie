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
    if (!pattern || patternId < 0) return;
    auto all = pattern->buildDrumPatternNotes(patternId);
    for (const auto& n : all) {
        int vr = rowOffset + numRows - 1 - n.note;
        if (vr >= 0 && vr < numRows)
            notes.push_back(n);
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

    // Horizontal row lines — uniform (no octave colouring)
    for (int i = 0; i <= numRows; i++) {
        fl_color(kRowLine);
        fl_line(x(), y() + i * rowHeight, x() + gridRight, y() + i * rowHeight);
    }

    // Vertical column lines
    for (int i = colOffset; i <= std::min(endCol, numCols); i++) {
        int x0 = x() + padX + (i - colOffset) * colWidth;
        // Bar lines come from the pattern's own time signature, as in PatternGrid.
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
        if (idx >= 0) {
            // Start drag on this note
            state = DrumStateDrag{
                idx,
                Fl::event_x(), Fl::event_y(),
                notes[idx].beat, notes[idx].note,
                false
            };
            if (window()) window()->cursor(FL_CURSOR_HAND);
        } else {
            state = DrumStateIdle{};
        }
        return 1;
    }

    case FL_DRAG: {
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
        if (auto* d = std::get_if<DrumStateDrag>(&state)) {
            if (d->moved && pattern) {
                // Capture before clearing state
                int   id       = notes[d->noteIdx].id;
                float beat     = notes[d->noteIdx].beat;
                int   midiNote = notes[d->noteIdx].note;
                float vel      = notes[d->noteIdx].velocity;
                state = DrumStateIdle{};  // clear BEFORE pattern ops so rebuild runs
                pattern->removeDrumNote(id);
                pattern->addDrumNote(patternId, midiNote, beat, vel);
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
            if (Fl::event_button() == FL_LEFT_MOUSE)
                createNote();
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
