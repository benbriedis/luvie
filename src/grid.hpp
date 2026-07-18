#ifndef GRID_HPP
#define GRID_HPP

#include "noteContextPopup.hpp"
#include "timeline.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <functional>
#include <variant>

class Playhead;

// Faint grey used for the beat-subdivision lines, drawn under the row lines.
constexpr Fl_Color subdivLineColor = 0xDDDDDD00;

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
    NoteContextPopup&            popup;
    std::vector<Note> notes;

    GridState state;
    bool      creationForbidden = false;

    Playhead* playhead  = nullptr;
    int       colOffset = 0;
    int       divisions = 1;  // beat subdivisions; 1 = None, so no extra lines

    bool isActiveDrag() const {
        return std::holds_alternative<StateDragMove>(state) ||
               std::holds_alternative<StateDragResize>(state);
    }

    void draw() override;
    int  handle(int event) override;
    void findNoteForCursor();
    virtual int  overlappingCell(int noteIdx) const;
    virtual void moving(StateDragMove& s);
    virtual void resizing(StateDragResize& s);
    void clampSelection();

    // Virtual row geometry — override in subclasses for variable-height rows
    virtual int rowY(int r) const         { return r * rowHeight; }
    virtual int rowH(int r) const         { (void)r; return rowHeight; }
    virtual int rowAtPixelY(int py) const { return rowHeight > 0 ? py / rowHeight : 0; }

    // Bottom y-extent of the drawn grid, relative to y(). Vertical column lines
    // stop here so they don't run past the last row into empty space below.
    virtual int gridBottom() const { return h(); }

    // Geometry of the note a click at beat position `fcol` would create: one
    // subdivision long, filling the subdivision cell the click landed in (or
    // starting at the click itself when snapping is off).
    virtual float newNoteLength() const { return 1.0f / (float)divisions; }
    virtual float newNoteStart(float fcol) const;

    // True when the click at `fcol` lands inside the note — i.e. it removes it.
    bool hitsNote(const Note& n, int row, float fcol) const {
        return (int)n.row == row && fcol >= n.beat && fcol < n.beat + n.length;
    }

    // Tolerance (in beats) for treating two note ranges as overlapping. Snapping
    // to non-power-of-two subdivisions (1/3, 1/5, 1/7) can't be represented
    // exactly in float, so an abutting note's start and its neighbour's end
    // differ by ~1e-7 beats; a strict comparison then reports a phantom overlap
    // and forbids the placement. This epsilon is far below any real subdivision
    // (1/7 ≈ 0.14) yet well above that rounding noise.
    static constexpr float kBeatEpsilon = 1e-4f;

    // True when beat-ranges [aStart, aStart+aLen) and [bStart, bStart+bLen)
    // overlap by more than kBeatEpsilon (so merely touching does not count).
    static bool beatsOverlap(float aStart, float aLen, float bStart, float bLen) {
        return aStart + kBeatEpsilon < bStart + bLen &&
               bStart + kBeatEpsilon < aStart + aLen;
    }

    // Virtual extension hooks
    virtual bool     isRowBlocked(int visualRow) const { (void)visualRow; return false; }
    virtual Fl_Color columnColor(int col)      const { (void)col;      return 0x00EE0000; }
    virtual Fl_Color rowLineColor(int lineIdx) const { (void)lineIdx;  return 0xEE888800; }
    virtual Fl_Color rowBgColor(int row)       const { (void)row;      return FL_WHITE; }
    virtual std::function<void()> makeDeleteCallback(int noteIdx) { (void)noteIdx; return nullptr; }
    virtual std::function<void(float)> makeVelocityCallback(int noteIdx) { (void)noteIdx; return nullptr; }
    virtual void openContextMenu(int idx);
    virtual void onBeginDrag(int noteIdx)               { (void)noteIdx; }
    virtual void onCommitMove(const StateDragMove& s)   { (void)s; }
    virtual void onCommitResize(const StateDragResize& s) { (void)s; }
    virtual void onNoteDoubleClick(int noteIdx)         { (void)noteIdx; }
    virtual void toggleNote();
    virtual void drawNoteBlock(const Note& note, int x0, int y0, int width, int rh);

public:
    Grid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);

    void setPlayhead(Playhead* p) { playhead  = p; }
    void setColOffset(int off)    { colOffset = off; redraw(); }
    int  getColOffset() const     { return colOffset; }
    void setSnap(float s)         { snap = s; }
    void setDivisions(int d)      { divisions = d > 1 ? d : 1; redraw(); }
};

#endif
