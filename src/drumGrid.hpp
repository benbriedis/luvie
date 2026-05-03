#ifndef DRUM_GRID_HPP
#define DRUM_GRID_HPP

#include "observablePattern.hpp"
#include "popup.hpp"
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
using DrumState = std::variant<DrumStateIdle, DrumStateHover, DrumStateDrag>;

// ---------------------------------------------------------------------------

class DrumGrid : public Fl_Box, public ITimelineObserver {
    ObservablePattern* pattern  = nullptr;
    int                 patternId = -1;
    int                 rowOffset = 0;   // MIDI note at the bottom visual row
    int                 colOffset = 0;
    float               snap;

    Popup&     popup;
    DrumState  state;

    // Local copy of notes in view — temporarily modified during drag
    std::vector<DrumNote> notes;

    void rebuildNotes();
    void createNote();          // called on left-release over empty space
    void removeNote(int idx);   // called on click (no drag) over existing note
    int  findNoteAtCursor() const;

    bool isActiveDrag() const {
        return std::holds_alternative<DrumStateDrag>(state);
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

    DrumGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup);
    void setPlayhead(Playhead* p) { playhead = p; }
    ~DrumGrid();

    void setPattern(ObservablePattern* tl, int patId);
    void setRowOffset(int offset);
    void setColOffset(int off) { colOffset = off; redraw(); }
    void setNumRows(int n)     { numRows   = n;   rebuildNotes(); }
    void onTimelineChanged()   override;

    int getRowOffset() const { return rowOffset; }
};

#endif
