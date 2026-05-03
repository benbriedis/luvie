#ifndef PIANOROLL_EDITOR_HPP
#define PIANOROLL_EDITOR_HPP

#include "basePatternEditor.hpp"
#include "pianorollGrid.hpp"
#include "popup.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

// Left-panel widget showing MIDI note names (C0, C#0, …) for each row
class PianorollLabels : public Fl_Widget {
    int numRows;
    int rowHeight;
    int rowOffset = 0;

    void draw() override;
    int  handle(int event) override;

public:
    std::function<void()> onRightClick;

    PianorollLabels(int x, int y, int w, int numRows, int rowHeight);
    void setRowOffset(int offset) { rowOffset = offset; redraw(); }
    void setNumRows(int n)        { numRows   = n;       redraw(); }
};

// ---------------------------------------------------------------------------

class PianorollEditor : public BasePatternEditor {
    static constexpr int labelsW = 70;

    PianorollLabels labels;
    PianorollGrid   grid;

    int  labelsWidth()      const override { return labelsW; }
    int  totalRows()        const override { return PianorollGrid::totalRows; }
    int  gridNumRows()      const override { return grid.numRows; }
    int  gridNumCols()      const override { return grid.numCols; }
    int  gridRowHeight()    const override { return grid.rowHeight; }
    int  gridColWidth()     const override { return grid.colWidth; }
    int  gridWidgetW()      const override { return grid.w(); }
    int  currentRowOffset() const override { return grid.getRowOffset(); }
    void gridSetRowOffset(int off) override { grid.setRowOffset(off); }
    void gridSetColOffset(int off) override { grid.setColOffset(off); }
    void gridSetNumRows(int n)     override { grid.setNumRows(n); }
    void gridResize(int x, int y, int w, int h) override { grid.resize(x, y, w, h); }
    void labelsSetRowOffset(int off) override { labels.setRowOffset(off); }
    void labelsSetNumRows(int n)     override { labels.setNumRows(n); }
    void labelsResize(int x, int y, int w, int h) override { labels.resize(x, y, w, h); }
    void labelsSetOnRightClick(std::function<void()> fn) override { labels.onRightClick = std::move(fn); }

    void setGridPattern(int patId) override;

public:
    PianorollEditor(int x, int y, int visibleW, int numRows, int numCols,
                    int rowHeight, int colWidth, float snap, Popup& popup);
    ~PianorollEditor();
};

#endif
