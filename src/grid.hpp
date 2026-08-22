// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef GRID_HPP
#define GRID_HPP

#include "noteContextPopup.hpp"
#include "selection.hpp"
#include "timeline.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <functional>
#include <variant>

class Playhead;

// Faint grey used for the beat-subdivision lines, drawn under the row lines.
constexpr Fl_Color subdivLineColor = 0xDDDDDD00;

// Rubber band and the outline on selected items. Amber reads clearly against
// both the blue velocity fills and the song editor's greys, and does not
// collide with the green/orange the tabs and playhead already use.
constexpr Fl_Color bandColor      = 0xF59E0B00;
constexpr Fl_Color selectionColor = 0xB4530900;
// Opacity of the band's fill, 0-255. Enough to read as a swept-out region, faint
// enough that the notes and grid lines under it stay legible.
constexpr unsigned char bandFillAlpha = 56;

class Selection;

// Draw the rubber band of `selection`, whose coordinates are relative to
// (originX, originY). Free-standing because DrumGrid is not a Grid but sweeps
// the same band.
void drawSelectionBand(const Selection& selection, int originX, int originY);

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

// Shift-drag: sweeping out a rubber band. `additive` keeps whatever was already
// selected, so several bands can build one selection.
struct StateBandSelect { bool additive; };

// Dragging a whole selection. The grabbed item is the primary — it is the one
// that follows the cursor under the usual snapping rules — and the delta it
// ends up with is applied to every other selected item. The primary is not
// named here: it may not be in `notes` at all (the song grid's automation dots),
// and everything the drag needs is its start position.
struct StateDragGroup {
    float grabX, grabY;
    Point original;      // primary's start position, so the delta is absolute
    // How far the selection may travel, resolved once when the drag begins.
    // See beginGroupDrag for why it is not recomputed as the drag proceeds.
    float minDBeat = 0.0f, maxDBeat = 0.0f;
    int   minDRow  = 0,    maxDRow  = 0;
    float dBeat = 0.0f;
    int   dRow  = 0;
    bool  blocked = false;   // the move would collide with something unselected
};

using GridState = std::variant<
    StateIdle, StateHoverMove, StateHoverResize, StateDragMove, StateDragResize,
    StateBandSelect, StateDragGroup>;

// ---------------------------------------------------------------------------

class Grid : public Fl_Box, public ISelectionHost {
public:
    int numRows, numCols;
    int rowHeight, colWidth;

protected:
    float             snap;
    NoteContextPopup&            popup;
    std::vector<Note> notes;

    GridState state;
    bool      creationForbidden = false;
    Selection selection;

    // Where each selected visible note sat when a group drag began, so the
    // preview can be recomputed from the original position on every mouse move
    // rather than accumulating rounding error. Indices into `notes`, which is
    // not rebuilt mid-drag (see isActiveDrag).
    struct GroupOrig { int idx; float beat; int row; };
    std::vector<GroupOrig> groupOrig;

    Playhead* playhead  = nullptr;
    int       colOffset = 0;
    int       divisions = 1;  // beat subdivisions; 1 = None, so no extra lines

    bool isActiveDrag() const {
        return std::holds_alternative<StateDragMove>(state)  ||
               std::holds_alternative<StateDragResize>(state) ||
               std::holds_alternative<StateDragGroup>(state)  ||
               std::holds_alternative<StateBandSelect>(state);
    }

    // ── Edge auto-scroll ─────────────────────────────────────────────────────
    // A drag held past the left or right edge scrolls the view after it, so a
    // selection can be dragged somewhere that isn't on screen when it starts.
    // The band sweep is deliberately left out: its anchor is a widget pixel, so
    // scrolling under it would slide the rectangle onto different bars.
    virtual bool isItemDrag() const {
        return std::holds_alternative<StateDragMove>(state)   ||
               std::holds_alternative<StateDragResize>(state)  ||
               std::holds_alternative<StateDragGroup>(state);
    }
    // Re-run the live drag against the current mouse position, after the view
    // has scrolled under it.
    virtual void reapplyDrag();
    // Cursor x, relative to the grid, for positioning whatever a drag is moving.
    // Once the view follows the drag out, a pointer past the edge means "keep
    // going", not "put it over there": pinning the position to the edge keeps
    // the dragged items on screen while the columns scroll under them. Grids
    // whose editor cannot scroll them keep the raw position, so a note can still
    // be dragged out to a column the view is not showing.
    float dragX() const;
    // Start, stop or reverse the scroll to match where the cursor now is. Call
    // on every drag event of a drag that should follow the cursor out.
    void updateEdgeScroll();
    void stopEdgeScroll();

    void draw() override;
    int  handle(int event) override;
    void findNoteForCursor();
    virtual int  overlappingCell(int noteIdx) const;
    virtual void moving(StateDragMove& s);
    virtual void resizing(StateDragResize& s);
    void clampSelection();

    // ── Multi-selection ──────────────────────────────────────────────────────
    // Grid owns the gesture (band, ctrl-click, group drag) and the preview; what
    // an item *is* and where the model keeps it stays with the subclass.

    // Every item id the model currently holds for this view — including rows
    // scrolled out of sight, which `notes` does not contain. Used to drop stale
    // ids after a change, and by the default select-all.
    virtual std::unordered_set<int> liveItemIds() const;
    // Select everything, reaching past the viewport (ctrl-A).
    virtual void selectAll();
    // Remove every selected item; one batched edit.
    virtual void deleteSelection();
    // How far the selection may be shifted before some member would leave the
    // grid. Computed over the whole selection from the model, not from the
    // primary and not from `notes` — otherwise a selection that reaches off
    // screen could be dragged out of range.
    virtual void groupDragLimits(float& minDBeat, float& maxDBeat,
                                 int& minDRow, int& maxDRow) const;
    // True if shifting the selection by this delta would collide with an item
    // that is NOT selected. Selected items keep their relative geometry, so they
    // can never newly collide with each other.
    virtual bool groupMoveBlocked(float dBeat, int dRow) const;
    // Apply the delta to every selected item, in one batched edit.
    virtual void onCommitGroupMove(float dBeat, int dRow);
    // Some grids lock an axis — the song editor moves instances sideways only.
    virtual bool allowsVerticalDrag() const { return true; }

    // Selected items are outlined rather than filled differently, so velocity
    // shading and the song editor's stacked-lane tinting stay readable under it.
    void drawSelectionOutline(int x0, int y0, int width, int rh) const;
    void drawBand() const;
    // Resolve the band into a selection and clear it.
    void applyBand(bool additive);
    // Runs while the band geometry is still live, for grids whose selectable
    // items are not all in `notes` (the song grid's automation dots).
    virtual void addBandHitExtras() {}
    // Preview the group drag for selected items that are not in `notes`, which
    // movingGroup shifts on its own. Called with 0 when the drag ends, to put
    // them back where the model says they are.
    virtual void previewGroupExtras(float dBeat) { (void)dBeat; }
    // Recompute the preview positions of the dragged selection.
    void movingGroup(StateDragGroup& s);
    // Shared by FL_PUSH: begin a group drag anchored on `noteIdx`.
    void beginGroupDrag(int noteIdx, float grabX, float grabY);
    // Same, for a primary that is not in `notes` — the caller passes its start
    // position and the grab offset within it.
    void beginGroupDrag(Point original, float grabX, float grabY);

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
    // Rows that suppress the vertical bar/subdivision lines (the playhead still draws over them)
    virtual bool     rowHidesColumnLines(int row) const { (void)row;      return false; }
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

private:
    static void edgeScrollTick(void* self);
    void edgeScrollStep();
    int  edgeScrollDir = 0;   // -1 left, +1 right, 0 not scrolling

public:
    Grid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~Grid() override;

    // Scroll the view by `cols` columns, returning how many it actually managed
    // (0 at either end). Set by the owning editor; a grid whose editor leaves it
    // unset simply does not auto-scroll.
    std::function<int(int cols)> onEdgeScroll;

    // ISelectionHost
    void clearSelection() override     { if (!selection.empty()) { selection.clear(); redraw(); } }
    void selectAllItems() override     { selectAll(); redraw(); }
    void deleteSelectedItems() override;
    bool hasSelection() const override { return !selection.empty(); }
    bool showing() const override      { return visible_r(); }
    bool ownsWindowPoint(int wx, int wy) const override
    { return wx >= x() && wx < x() + w() && wy >= y() && wy < y() + h(); }

    void setPlayhead(Playhead* p) { playhead  = p; }
    void setColOffset(int off)    { colOffset = off; redraw(); }
    int  getColOffset() const     { return colOffset; }
    void setSnap(float s)         { snap = s; }
    void setDivisions(int d)      { divisions = d > 1 ? d : 1; redraw(); }
};

#endif
