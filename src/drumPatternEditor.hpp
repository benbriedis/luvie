#ifndef DRUM_PATTERN_EDITOR_HPP
#define DRUM_PATTERN_EDITOR_HPP

#include "basePatternEditor.hpp"
#include "drumGrid.hpp"
#include "popup.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Widget.H>
#include <functional>
#include <map>
#include <string>

// Simple left-panel widget that shows MIDI note numbers (or instrument names) for each row
class DrumNoteLabels : public Fl_Widget {
    int numRows;
    int rowHeight;
    int rowOffset         = 0;
    bool fallbackNoteNames = false;
    std::map<int, std::string> drumMap;

    void draw() override;
    int  handle(int event) override;

public:
    DrumNoteLabels(int x, int y, int w, int numRows, int rowHeight);
    void setRowOffset(int offset) { rowOffset = offset; redraw(); }
    void setNumRows(int n)        { numRows   = n;       redraw(); }
    void setDrumMap(const std::map<int, std::string>& m) { drumMap = m; redraw(); }
    void setFallbackNoteNames(bool b) { fallbackNoteNames = b; redraw(); }

    std::function<void(int midiNote, int rowY, int rowH)> onRowDoubleClicked;
    std::function<void()> onRightClick;
};

// ---------------------------------------------------------------------------

class DrumPatternEditor : public BasePatternEditor {
    static constexpr int labelsW = 90;

    DrumNoteLabels drumLabels;
    DrumGrid       drumGrid;
    InlineInput    drumLabelInput;
    int            editingMidiNote = -1;

    std::map<std::string, std::map<int, std::string>> allDrumMaps;
    std::map<std::string, bool>                       allFallbackModes;

    std::string currentInstrumentName() const;
    void applyCurrentDrumMap();
    void startDrumLabelEdit(int midiNote, int rowY, int rowH);
    void commitDrumLabelEdit();
    void cancelDrumLabelEdit();
    int  handle(int event) override;

    int  labelsWidth()      const override { return labelsW; }
    int  totalRows()        const override { return DrumGrid::totalRows; }
    int  gridNumRows()      const override { return drumGrid.numRows; }
    int  gridNumCols()      const override { return drumGrid.numCols; }
    int  gridRowHeight()    const override { return drumGrid.rowHeight; }
    int  gridColWidth()     const override { return drumGrid.colWidth; }
    int  gridWidgetW()      const override { return drumGrid.w(); }
    int  currentRowOffset() const override { return drumGrid.getRowOffset(); }
    void gridSetRowOffset(int off) override { drumGrid.setRowOffset(off); }
    void gridSetColOffset(int off) override { drumGrid.setColOffset(off); }
    void gridSetNumRows(int n)     override { drumGrid.setNumRows(n); }
    void gridResize(int x, int y, int w, int h) override { drumGrid.resize(x, y, w, h); }
    void labelsSetRowOffset(int off) override { drumLabels.setRowOffset(off); }
    void labelsSetNumRows(int n)     override { drumLabels.setNumRows(n); }
    void labelsResize(int x, int y, int w, int h) override { drumLabels.resize(x, y, w, h); }
    void labelsSetOnRightClick(std::function<void()> fn) override { drumLabels.onRightClick = std::move(fn); }

    void setGridPattern(int patId) override;
    void afterTimelineChanged(int patId) override;

public:
    DrumPatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                      int rowHeight, int colWidth, float snap, Popup& popup);
    ~DrumPatternEditor();

    std::function<void(const std::string& chanName, int midiNote, const std::string& label)> onDrumLabelChanged;

    void focusPattern() override;
    void setAllDrumMaps(const std::map<std::string, std::map<int, std::string>>& maps,
                        const std::map<std::string, bool>& fallbacks);
    void resize(int x, int y, int w, int h) override;
};

#endif
