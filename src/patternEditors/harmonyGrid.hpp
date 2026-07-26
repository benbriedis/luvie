#ifndef HARMONY_GRID_HPP
#define HARMONY_GRID_HPP

#include "grid.hpp"
#include "observablePattern.hpp"
#include <vector>
#include <functional>
#include <set>
#include <optional>

class HarmonyGrid : public Grid, public ITimelineObserver {
    ObservablePattern* pattern        = nullptr;
    int                 patternId       = -1;
    int                 chordSize       = 3;
    int                 rowOffset       = 0;   // in virtual-row units
    std::vector<int>    bonusDegrees;          // sorted ascending; unique bonus degrees
    int                 pitchGroupSize  = 3;   // chordSize + bonusDegrees.size()
    int                 totalTones      = 0;   // rows the labels show, in virtual-row units

    struct RapidCell {
        int row, col;
        bool operator==(const RapidCell& o) const { return row == o.row && col == o.col; }
    };

    bool                         rapidMode           = false;
    bool                         rapidRemovedOnClick = false;
    std::set<std::pair<int,int>> rapidCells;
    std::optional<RapidCell>     rapidLast;
    std::optional<RapidCell>     rapidPending;

    void rebuildNotes();
    void addNoteAt(int virtualPos, float col, float length);
    bool screenToCell(int ex, int ey, int& outRow, int& outAbsCol) const;
    void rapidTryCreate(int visualRow, int absCol);
    void processRapidCell(RapidCell cur);
    static bool rapidIsDiagonal(RapidCell a, RapidCell b) {
        return std::abs(a.row - b.row) == 1 && std::abs(a.col - b.col) == 1;
    }

    // Convert a virtual row index to chord-space abs_row (-1 if it's a bonus row)
    int virtualToAbsRow(int virtualPos) const;

    // Whether a virtual row is one the user can put a note on at all
    bool validVirtualPos(int virtualPos) const;

    // Virtual row a stored note occupies (-1 if its bonus degree is gone),
    // and the note-slot a virtual row stands for — inverses of each other.
    int virtualPosOf(const Note& n) const;
    ObservablePattern::NoteRowSlot slotForVirtualPos(int noteId, int virtualPos) const;
    std::pair<int,int> virtualPosExtent() const;
    void transposeRows(int rows);

protected:
    Fl_Color columnColor(int col) const override;
    Fl_Color rowLineColor(int i)  const override;
    Fl_Color rowBgColor(int row)  const override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    std::function<void(float)> makeVelocityCallback(int noteIdx) override;
    std::function<void(int,int)> makeTransposeCallback(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void toggleNote() override;

    int handle(int event) override;

public:
    // Fired on every rebuild; args: (bonusDegrees, pitchGroupSize)
    std::function<void(const std::vector<int>&, int)> onBonusDegreesChanged;

    HarmonyGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~HarmonyGrid();

    void setPattern(ObservablePattern* tl, int patId);
    void setChordSize(int size) { chordSize = size; pitchGroupSize = size + (int)bonusDegrees.size(); redraw(); }
    void setTotalTones(int t)   { totalTones = t; }
    void setNumRows(int n) { numRows = n; rebuildNotes(); }
    void setRowOffset(int offset);
    void setRapidMode(bool r);
    void onTimelineChanged() override;

    int getPitchGroupSize()  const { return pitchGroupSize; }
    int getChordSize()       const { return chordSize; }
    int getRowOffset()       const { return rowOffset; }
};

#endif
