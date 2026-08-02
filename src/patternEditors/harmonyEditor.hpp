// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef HARMONY_EDITOR_HPP
#define HARMONY_EDITOR_HPP

#include "basePatternEditor.hpp"
#include "harmonyGrid.hpp"
#include "harmonyLabels.hpp"
#include "noteContextPopup.hpp"
#include <vector>

class HarmonyEditor : public BasePatternEditor {
    static constexpr int labelsW = 70;

    HarmonyLabels  harmonyLabels;
    HarmonyGrid harmonyGrid;
    int         rootPitch       = 0;
    int         chordIndex      = 0;   // resolved from the pattern's chord hash

    int  computeDefaultOffset(int patId) const;

    int  labelsWidth()      const override { return labelsW; }
    int  totalRows()        const override { return harmonyLabels.getTotalTones(); }
    int  gridNumRows()      const override { return harmonyGrid.numRows; }
    int  gridNumCols()      const override { return harmonyGrid.numCols; }
    int  gridRowHeight()    const override { return harmonyGrid.rowHeight; }
    int  gridColWidth()     const override { return harmonyGrid.colWidth; }
    int  gridWidgetW()      const override { return harmonyGrid.w(); }
    int  currentRowOffset() const override { return harmonyLabels.getRowOffset(); }
    void gridSetRowOffset(int off) override { harmonyGrid.setRowOffset(off); }
    void gridSetColOffset(int off) override { harmonyGrid.setColOffset(off); }
    void gridSetColWidth(int cw)   override { harmonyGrid.colWidth = cw; }
    void gridSetNumRows(int n)     override { harmonyGrid.setNumRows(n); }
    void gridSetNumCols(int n)     override { harmonyGrid.numCols = n; }
    void gridResize(int x, int y, int w, int h) override { harmonyGrid.resize(x, y, w, h); }
    void labelsSetRowOffset(int off) override { harmonyLabels.setRowOffset(off); }
    void labelsSetNumRows(int n)     override { harmonyLabels.setNumRows(n); }
    void labelsResize(int x, int y, int w, int h) override { harmonyLabels.resize(x, y, w, h); }
    void labelsSetOnRightClick(std::function<void()> fn) override { harmonyLabels.onRightClick = std::move(fn); }
    void labelsSetOnRowClicked(std::function<void(int)> fn) override { harmonyLabels.onRowClicked = std::move(fn); }

    void setGridPattern(int patId) override;

public:
    HarmonyEditor(int x, int y, int visibleW, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~HarmonyEditor();

    void setNoteParams(int rootPitch, std::string_view chordHash, bool useSharp);
    int  numPatternBeats() const { return harmonyGrid.numCols; }
    void focusPattern() override;
    void setSnap(float s) override { harmonyGrid.setSnap(s); BasePatternEditor::setSnap(s); }
    void setDivisions(int d) override { harmonyGrid.setDivisions(d); }
    void setRapidMode(bool r)      { harmonyGrid.setRapidMode(r); }
    void setTransposePopup(TransposePopup* p) { harmonyGrid.setTransposePopup(p); }
};

#endif
