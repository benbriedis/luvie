#ifndef GRID_HPP
#define GRID_HPP

#include "popup.hpp"
#include "timeline.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <functional>
#include <variant>

class Playhead;

enum class Side { Left, Right };
struct Point { int row; float col; };

// ---------------------------------------------------------------------------
// Interaction state — each variant carries only the fields relevant to it.
// ---------------------------------------------------------------------------

struct StateIdle {};

struct StateHoverMove   { int noteIdx; float grabX, grabY; };
struct StateHoverResize { int noteIdx; Side side; };

struct StateDragMove {
    int   noteIdx;
    float grabX, grabY;
    Point original;
    Point lastValid;
    bool  overlapping = false;
};

struct StateDragResize { int noteIdx; Side side; };

using GridState = std::variant<
    StateIdle, StateHoverMove, StateHoverResize, StateDragMove, StateDragResize>;

// ---------------------------------------------------------------------------

class Grid : public Fl_Box {
public:
    int numRows, numCols;
    int rowHeight, colWidth;

protected:
    float             snap;
    Popup&            popup;
    std::vector<Note> notes;

    GridState state;
    bool      creationForbidden = false;

    Playhead* playhead  = nullptr;
    int       colOffset = 0;

    bool isActiveDrag() const {
        return std::holds_alternative<StateDragMove>(state) ||
               std::holds_alternative<StateDragResize>(state);
    }

    void draw() override;
    int  handle(int event) override;
    void findNoteForCursor();
    int  overlappingNote(int noteIdx) const;
    void moving(StateDragMove& s);
    virtual void resizing(StateDragResize& s);
    void clampSelection();

    // Virtual extension hooks
    virtual Fl_Color columnColor(int col)      const { (void)col;      return 0x00EE0000; }
    virtual Fl_Color rowLineColor(int lineIdx) const { (void)lineIdx;  return 0xEE888800; }
    virtual Fl_Color rowBgColor(int row)       const { (void)row;      return FL_WHITE; }
    virtual std::function<void()> makeDeleteCallback(int noteIdx) { (void)noteIdx; return nullptr; }
    virtual void onBeginDrag(int noteIdx)               { (void)noteIdx; }
    virtual void onCommitMove(const StateDragMove& s)   { (void)s; }
    virtual void onCommitResize(const StateDragResize& s) { (void)s; }
    virtual void onNoteDoubleClick(int noteIdx)         { (void)noteIdx; }
    virtual void toggleNote();

public:
    Grid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup);

    void setPlayhead(Playhead* p) { playhead  = p; }
    void setColOffset(int off)    { colOffset = off; redraw(); }
};

#endif
