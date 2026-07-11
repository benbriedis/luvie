#ifndef PATTERN_EDITOR_HPP
#define PATTERN_EDITOR_HPP

#include "basePatternEditor.hpp"
#include "patternGrid.hpp"
#include "noteLabels.hpp"
#include "noteContextPopup.hpp"
#include <vector>

class PatternEditor : public BasePatternEditor {
    static constexpr int labelsW = 70;

    NoteLabels  noteLabels;
    PatternGrid patternGrid;
    float       lastLengthBeats = -1.0f;
    int         rootPitch       = 0;
    int         chordIndex      = 0;   // resolved from the pattern's chord hash

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
    void gridSetColWidth(int cw)   override { patternGrid.colWidth = cw; }
    void gridSetNumRows(int n)     override { patternGrid.setNumRows(n); }
    void gridResize(int x, int y, int w, int h) override { patternGrid.resize(x, y, w, h); }
    void labelsSetRowOffset(int off) override { noteLabels.setRowOffset(off); }
    void labelsSetNumRows(int n)     override { noteLabels.setNumRows(n); }
    void labelsResize(int x, int y, int w, int h) override { noteLabels.resize(x, y, w, h); }
    void labelsSetOnRightClick(std::function<void()> fn) override { noteLabels.onRightClick = std::move(fn); }
    void labelsSetOnRowClicked(std::function<void(int)> fn) override { noteLabels.onRowClicked = std::move(fn); }

    void setGridPattern(int patId) override;
    void afterTimelineChanged(int patId) override;

public:
    PatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~PatternEditor();

    void setNoteParams(int rootPitch, std::string_view chordHash, bool useSharp);
    int  numPatternBeats() const { return patternGrid.numCols; }
    void focusPattern() override;
    void setSnap(float s) override { patternGrid.setSnap(s); BasePatternEditor::setSnap(s); }
    void setRapidMode(bool r)      { patternGrid.setRapidMode(r); }
};

#endif
