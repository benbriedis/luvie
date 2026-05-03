#ifndef PATTERN_EDITOR_HPP
#define PATTERN_EDITOR_HPP

#include "basePatternEditor.hpp"
#include "patternGrid.hpp"
#include "noteLabels.hpp"
#include "popup.hpp"
#include <vector>

class PatternEditor : public BasePatternEditor {
    static constexpr int labelsW = 70;

    NoteLabels  noteLabels;
    PatternGrid patternGrid;
    float       lastLengthBeats = -1.0f;
    int         rootPitch       = 0;
    int         chordType       = 0;

    int  computeDefaultOffset(int patId) const;

    int  labelsWidth()      const override { return labelsW; }
    int  totalRows()        const override { return noteLabels.getTotalTones(); }
    int  gridNumRows()      const override { return patternGrid.numRows; }
    int  gridNumCols()      const override { return patternGrid.numCols; }
    int  gridRowHeight()    const override { return patternGrid.rowHeight; }
    int  gridColWidth()     const override { return patternGrid.colWidth; }
    int  gridWidgetW()      const override { return patternGrid.w(); }
    int  currentRowOffset() const override { return noteLabels.getRowOffset(); }
    void gridSetRowOffset(int off) override { patternGrid.setRowOffset(off); }
    void gridSetColOffset(int off) override { patternGrid.setColOffset(off); }
    void gridSetNumRows(int n)     override { patternGrid.setNumRows(n); }
    void gridResize(int x, int y, int w, int h) override { patternGrid.resize(x, y, w, h); }
    void labelsSetRowOffset(int off) override { noteLabels.setRowOffset(off); }
    void labelsSetNumRows(int n)     override { noteLabels.setNumRows(n); }
    void labelsResize(int x, int y, int w, int h) override { noteLabels.resize(x, y, w, h); }
    void labelsSetOnRightClick(std::function<void()> fn) override { noteLabels.onRightClick = std::move(fn); }

    void setGridPattern(int patId) override;
    void afterTimelineChanged(int patId) override;

public:
    PatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, Popup& popup);
    ~PatternEditor();

    void setNoteParams(int rootPitch, int chordType, bool useSharp);
    int  numPatternBeats() const { return patternGrid.numCols; }
    void focusPattern() override;
};

#endif
