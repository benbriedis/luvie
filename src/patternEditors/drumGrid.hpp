// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef DRUM_GRID_HPP
#define DRUM_GRID_HPP

#include "observablePattern.hpp"
#include "noteContextPopup.hpp"
#include "selection.hpp"
#include <FL/Fl_Box.H>
#include <vector>
#include <variant>

class Playhead;

// ---------------------------------------------------------------------------
// Interaction states
// ---------------------------------------------------------------------------
struct DrumStateIdle {};
struct DrumStateHover { int noteIdx; };
struct DrumStateDrag {
    int   noteIdx;
    int   grabX;        // pixel x where drag started
    int   grabY;        // pixel y where drag started
    float origBeat;
    int   origNote;     // original MIDI note
    bool  moved = false;
};
// Shift-drag sweeps a rubber band; `additive` keeps the existing selection.
struct DrumStateBand { bool additive; };

// Dragging a whole selection. The grabbed note is the primary and follows the
// cursor; the delta it lands on is applied to every other selected note.
struct DrumStateDragGroup {
    int   grabX, grabY;     // pixel position the drag was anchored at
    float origBeat;
    int   origNote;
    float dBeat   = 0.0f;
    int   dNote   = 0;
    bool  blocked = false;
};

using DrumState = std::variant<DrumStateIdle, DrumStateHover, DrumStateDrag,
                               DrumStateBand, DrumStateDragGroup>;

// ---------------------------------------------------------------------------

class DrumGrid : public Fl_Box, public ITimelineObserver, public ISelectionHost {
    ObservablePattern* pattern  = nullptr;
    int                 patternId = -1;
    int                 rowOffset = 0;   // MIDI note at the bottom visual row
    int                 colOffset = 0;
    int                 padX      = 0;   // left margin (px) before beat 0, so the
                                         // leftmost note circle isn't clipped
    float               snap;
    int                 divisions = 1;  // beat subdivisions; 1 = None, so no extra lines

    NoteContextPopup&     popup;
    DrumState  state;
    Selection  selection;

    // Set by a press that must not create a note when the button comes back up —
    // a ctrl-click, or a plain click that only dismissed a selection. Cleared on
    // release, mirroring Grid.
    bool creationForbidden = false;

    // Local copy of notes in view — temporarily modified during drag
    std::vector<DrumNote> notes;

    // Where each selected visible note sat when a group drag began, so the
    // preview is recomputed from the original rather than accumulating drift.
    struct GroupOrig { int idx; float beat; int note; };
    std::vector<GroupOrig> groupOrig;

    void rebuildNotes();
    void createNote();          // called on left-release over empty space
    void removeNote(int idx);   // called on click (no drag) over existing note
    int  findNoteAtCursor() const;

    // Selection, mirroring Grid's but over DrumNote — point events with a MIDI
    // note rather than ranges with a row, so none of Grid's geometry applies.
    void selectAllNotes();
    void deleteSelection();
    void groupDragLimits(float& minDBeat, float& maxDBeat, int& minDNote, int& maxDNote) const;
    bool groupMoveBlocked(float dBeat, int dNote) const;
    void commitGroupMove(float dBeat, int dNote);
    void beginGroupDrag(int idx, int grabX, int grabY);
    void movingGroup(DrumStateDragGroup& d);
    void applyBand(bool additive);
    int  dotRadius() const { return std::max(2, rowHeight / 3); }

    bool isActiveDrag() const {
        return std::holds_alternative<DrumStateDrag>(state) ||
               std::holds_alternative<DrumStateDragGroup>(state) ||
               std::holds_alternative<DrumStateBand>(state);
    }

protected:
    void draw()          override;
    int  handle(int evt) override;

public:
    static constexpr int totalRows = 128;  // MIDI notes 0–127

    int numRows;
    int numCols;
    int rowHeight;
    int colWidth;

    Playhead* playhead = nullptr;

    DrumGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    void setPlayhead(Playhead* p) { playhead = p; }
    ~DrumGrid();

    void setPattern(ObservablePattern* tl, int patId);
    void setRowOffset(int offset);
    void setColOffset(int off) { colOffset = off; redraw(); }
    void setNumRows(int n)     { numRows   = n;   rebuildNotes(); }
    void setSnap(float s)      { snap = s; }
    void setDivisions(int d)   { divisions = d > 1 ? d : 1; redraw(); }
    void onTimelineChanged()   override;

    // ISelectionHost
    void clearSelection() override     { if (!selection.empty()) { selection.clear(); redraw(); } }
    void selectAllItems() override     { selectAllNotes(); redraw(); }
    void deleteSelectedItems() override;
    bool hasSelection() const override { return !selection.empty(); }
    bool showing() const override      { return visible_r(); }
    bool ownsWindowPoint(int wx, int wy) const override
    { return wx >= x() && wx < x() + w() && wy >= y() && wy < y() + h(); }

    int getRowOffset() const { return rowOffset; }
    int getPadX() const { return padX; }
};

#endif
