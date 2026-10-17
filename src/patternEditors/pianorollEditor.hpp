// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PIANOROLL_EDITOR_HPP
#define PIANOROLL_EDITOR_HPP

#include "basePatternEditor.hpp"
#include "pianorollGrid.hpp"
#include "noteContextPopup.hpp"
#include "inlineInput.hpp"
#include "liveNoteLights.hpp"
#include <FL/Fl_Widget.H>
#include <functional>
#include <map>
#include <string>

// Left-panel widget showing MIDI note names (C0, C#0, …) for each row, or the
// instrument's own name for a note where it has one
class PianorollLabels : public Fl_Widget {
    int numRows;
    int rowHeight;
    int rowOffset = 0;
    int flashMidi = -1;   // MIDI pitch briefly lit after a click
    std::map<int, std::string> noteNames;
    bool fallbackNoteNames = true;   // unnamed rows: note names, else MIDI numbers

    void        flash(int midi);
    static void clearFlashCb(void* self);
    void draw() override;
    int  handle(int event) override;

public:
    std::function<void()>    onRightClick;
    std::function<void(int)> onRowClicked;   // visual row clicked → MIDI pitch
    LiveNoteLights           liveNotes{this}; // notes arriving on the MIDI input

    PianorollLabels(int x, int y, int w, int numRows, int rowHeight);
    ~PianorollLabels();
    void setRowOffset(int offset) { rowOffset = offset; redraw(); }
    void setNumRows(int n)        { numRows   = n;       redraw(); }
    void setNoteNames(const std::map<int, std::string>& m) { noteNames = m; redraw(); }
    void setFallbackNoteNames(bool b) { fallbackNoteNames = b; redraw(); }

    // Resolve an absolute y to the row it falls on: its MIDI pitch and the
    // row's screen rectangle. Returns false outside the labelled range.
    bool rowAt(int ey, int& midiNote, int& rowY, int& rowH) const;
};

// ---------------------------------------------------------------------------

class PianorollEditor : public BasePatternEditor {
    static constexpr int labelsW = 70;

    PianorollLabels labels;
    PianorollGrid   grid;
public:
    ISelectionHost* selectionHost() { return &grid; }
private:
    InlineInput     labelInput;
    int             editingMidiNote = -1;

    std::map<int, std::map<int, std::string>> allNoteNames;

    void applyCurrentNoteNames();
    void startLabelEdit(int midiNote, int rowY, int rowH);
    void commitLabelEdit();
    void cancelLabelEdit();
    int  handle(int event) override;

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
    void gridSetColWidth(int cw)   override { grid.colWidth = cw; }
    void gridSetNumRows(int n)     override { grid.setNumRows(n); }
    void gridSetNumCols(int n)     override { grid.numCols = n; }
    void gridResize(int x, int y, int w, int h) override { grid.resize(x, y, w, h); }
    void labelsSetRowOffset(int off) override { labels.setRowOffset(off); }
    void labelsSetNumRows(int n)     override { labels.setNumRows(n); }
    void labelsResize(int x, int y, int w, int h) override { labels.resize(x, y, w, h); }
    void labelsSetOnRightClick(std::function<void()> fn) override { labels.onRightClick = std::move(fn); }
    void labelsSetOnRowClicked(std::function<void(int)> fn) override { labels.onRowClicked = std::move(fn); }
    std::function<void()> labelsRenameHandler() override;
    bool labelsHaveRowLabelsToggle() const override { return true; }
    LiveNoteLights& labelsLiveNotes() override { return labels.liveNotes; }

    void setGridPattern(int patId) override;
    void afterTimelineChanged(int patId) override;

public:
    void focusPattern() override;
    void setSnap(float s) override { grid.setSnap(s); BasePatternEditor::setSnap(s); }
    void setDivisions(int d) override { grid.setDivisions(d); }
    PianorollEditor(int x, int y, int visibleW, int numRows, int numCols,
                    int rowHeight, int colWidth, float snap, NoteContextPopup& popup);
    ~PianorollEditor();

    std::function<void(int instrId, int midiNote, const std::string& label)> onNoteLabelChanged;

    // Each instrument's note names, shared with the drum editor's drum maps.
    void setAllNoteNames(const std::map<int, std::map<int, std::string>>& names);
    void setNoteName(int instrId, int midiNote, const std::string& label);
    void resize(int x, int y, int w, int h) override;
};

#endif
