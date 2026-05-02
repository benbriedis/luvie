#ifndef DRUM_PATTERN_EDITOR_HPP
#define DRUM_PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "drumGrid.hpp"
#include "patternParamGrid.hpp"
#include "noteLabelsContextPopup.hpp"
#include "paramDotPopup.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include "gridScrollPane.hpp"
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

class DrumPatternEditor : public Editor, public ITimelineObserver {
    static constexpr int labelsW    = 90;
    static constexpr int scrollbarW = 14;

    GridScrollPane*     scrollbar         = nullptr;
    GridScrollPane*     paramScrollbar    = nullptr;
    DrumNoteLabels      drumLabels;
    DrumGrid            drumGrid;
    InlineInput         drumLabelInput;
    PatternParamLabels  paramLabels;
    PatternParamGrid    paramGrid;
    ObservableTimeline* timeline          = nullptr;
    int                 lastSelectedTrack = -1;
    int                 colOffset         = 0;
    int                 editingMidiNote   = -1;
    int                 paramLaneOffset   = 0;

    std::map<std::string, std::map<int, std::string>> allDrumMaps;
    std::map<std::string, bool>                       allFallbackModes;

    std::string currentInstrumentName() const;
    void setRowOffset(int offset);
    void setColOffset(int offset);
    void applyCurrentDrumMap();
    void startDrumLabelEdit(int midiNote, int rowY, int rowH);
    void commitDrumLabelEdit();
    void cancelDrumLabelEdit();
    void updateParamScrollbar();
    int  handle(int event) override;

public:
    DrumPatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                      int rowHeight, int colWidth, float snap, Popup& popup);
    ~DrumPatternEditor();

    std::function<void(const std::string& chanName, int midiNote, const std::string& label)> onDrumLabelChanged;

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    void setAllDrumMaps(const std::map<std::string, std::map<int, std::string>>& maps,
                        const std::map<std::string, bool>& fallbacks);
    void onTimelineChanged() override;
    void resize(int x, int y, int w, int h) override;
};

#endif
