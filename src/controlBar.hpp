// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef CONTROL_BAR_HPP
#define CONTROL_BAR_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <functional>
#include <vector>

// ---------------------------------------------------------------------------
// Control-bar layout description
//
// A bar is described as an ordered list of rows; each row packs some items from
// its left edge and some against its right edge. Deciding how the controls are
// distributed over the rows is the subclass's buildLayout() alone — placing them
// is generic — so a different fold arrangement is a change to that one function.
// ---------------------------------------------------------------------------

// One control in the bar, with the width it wants.
struct PanelItem {
    Fl_Widget* widget;
    int        width;
    int        gapBefore = -1;   // space before this item; -1 = the standard gap
};

struct PanelRow {
    std::vector<PanelItem> left;        // packed from the left edge
    std::vector<PanelItem> right;       // packed against the right edge
    int                    indent = 0;  // extra inset before the left run
};

// The dark strip of controls that sits at the bottom of an editor tab, just
// above the transport bar. It owns the row geometry, the placement of the items
// a subclass hands it, and the height it asks its owner for; what those items
// are and how they fold is the subclass's business.
class ControlBar : public Fl_Group {
protected:
    // Row geometry. One row is a strip of rowH; a folded bar stacks two of them.
    static constexpr int rowH    = 28;
    static constexpr int vMargin = 2;   // above the first row / below the last
    static constexpr int rowGap  = 2;
    static constexpr int pad     = 3;   // inset at either end of a row
    static constexpr int itemGap = 3;   // standard space between two items
    static constexpr int ctrlH   = 24;  // height of a control within its row

    // Which controls sit on which row at this width.
    virtual std::vector<PanelRow> buildLayout(int availW) = 0;
    // Runs once the rows have been placed, for anything that has to follow a
    // control it overlays (an inline editor, say).
    virtual void afterLayout() {}

    // The width this row needs, hidden items excluded (they take no slot).
    static int rowWidth(const PanelRow& row);
    static int rowsHeight(int rows) { return 2*vMargin + rows*rowH + (rows - 1)*rowGap; }

    void applyLayout(const std::vector<PanelRow>& rows);
    void relayout();

    void draw() override;

public:
    ControlBar(int x, int y, int w, int h) : Fl_Group(x, y, w, h) { box(FL_NO_BOX); }

    // Fired when the bar folds or unfolds and so wants a different height; the
    // owner re-fits whatever sits above the bar.
    std::function<void(int)> onHeightChanged;

    // Height the bar needs at this width (one row, or more once it folds).
    int  heightForWidth(int width) { return rowsHeight((int)buildLayout(width).size()); }

    void resize(int x, int y, int w, int h) override;
};

#endif
