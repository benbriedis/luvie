// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef BASE_PATTERN_EDITOR_HPP
#define BASE_PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "gridPane.hpp"
#include "patternParamGrid.hpp"
#include "noteLabelsContextPopup.hpp"
#include "paramDotPopup.hpp"
#include "itransport.hpp"
#include "observablePattern.hpp"
#include "gridScrollPane.hpp"
#include "sliceController.hpp"
#include "liveNoteLights.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

class NoteAuditioner;

class BasePatternEditor : public Editor, public ITimelineObserver {
protected:
    static constexpr int scrollbarW = 14;

    GridPane           gridPane;
    GridScrollPane*    scrollbar      = nullptr;
    GridScrollPane*    paramScrollbar = nullptr;
    PatternParamLabels paramLabels;
    PatternParamGrid   paramGrid;
    // The time slice, shared by the note or drum grid and the automation lanes;
    // each subclass hands it to its grid. See SliceController.
    SliceController    sliceCtl;
    ObservablePattern* pattern             = nullptr;
    NoteAuditioner*    auditioner          = nullptr;
    int                lastSelectedTrack  = -1;
    int                lastSelectedLaneId = -1;
    int                lastPatId          = -1;
    int                colOffset         = 0;
    int                paramLaneOffset   = 0;
    int                baseColWidth      = 0;   // colWidth at zoom x1
    float              lastLengthBeats   = -1.0f;
    bool               wasFollowing      = false;   // followPlayhead() ran last tick
    float              lastHeadBeat      = 0.0f;

    // Subclass grid geometry — all are one-liners forwarding to the concrete grid/labels
    virtual int  labelsWidth()      const = 0;
    virtual int  totalRows()        const = 0;
    virtual int  gridNumRows()      const = 0;
    virtual int  gridNumCols()      const = 0;
    virtual int  gridRowHeight()    const = 0;
    virtual int  gridColWidth()     const = 0;
    virtual int  gridWidgetW()      const = 0;
    virtual int  currentRowOffset() const = 0;  // from labels (HarmonyEditor) or grid (others)
    virtual void gridSetRowOffset(int offset)             = 0;
    virtual void gridSetColOffset(int offset)             = 0;
    virtual void gridSetColWidth(int colWidth)            = 0;
    virtual void gridSetNumRows(int n)                    = 0;
    virtual void gridSetNumCols(int n)                    = 0;
    virtual void gridResize(int x, int y, int w, int h)   = 0;
    virtual void labelsSetRowOffset(int offset)           = 0;
    virtual void labelsSetNumRows(int n)                  = 0;
    virtual void labelsResize(int x, int y, int w, int h) = 0;
    virtual void labelsSetOnRightClick(std::function<void()> fn) = 0;
    virtual void labelsSetOnRowClicked(std::function<void(int midi)> fn) = 0;
    // Editors whose labels can be renamed (the drum editor) return a closure
    // that renames the row under the current event; others leave it empty and
    // the context menu omits its "Rename" item.
    virtual std::function<void()> labelsRenameHandler() { return {}; }
    // The label column's lights for notes arriving on the MIDI input.
    virtual LiveNoteLights& labelsLiveNotes() = 0;

    // onTimelineChanged skeleton hooks
    virtual void setGridPattern(int patId)    = 0;
    virtual void afterTimelineChanged(int /*patId*/) {}

    void setRowOffset(int offset);
    void setColOffset(int offset);
    // Per playhead tick: page the grid so a playing head stays in view.
    void followPlayhead();
    void applyPatternLength(int patId);
    void updateParamScrollbar();
    void relayout();
    void layoutBody() override { relayout(); }
    void onWheelX(int d) override { setColOffset(colOffset + d); }
    void onWheelY(int d) override { setRowOffset(currentRowOffset() - d); }
    // Tells the param menu which instrument pattern patId plays.
    void setParamTarget(NoteLabelsContextPopup* popup, int patId) const;

    BasePatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                      int rowHeight, int colWidth, float snap, int lw);

public:
    ~BasePatternEditor();

    virtual void focusPattern() {}
    virtual void setSnap(float s) { paramGrid.setSnap(s); sliceCtl.setSnap(s); }
    // Beat subdivisions (1 = None): drawn as faint grid lines, independent of snapping.
    virtual void setDivisions(int d) { (void)d; }
    // Horizontal zoom: `factor` scales the column width from its x1 base (so 0.2
    // through 4). Note minimum pixel widths are unaffected, so shorter notes
    // remain creatable when zoomed.
    void setZoom(float factor);
    void setPatternPlayhead(ITransport* t, ObservablePattern* pat, int trackIndex);
    void setAuditioner(NoteAuditioner* a);

    // ── MIDI input ───────────────────────────────────────────────────────────
    // Notes arriving on the MIDI input light their label rows, which is how a pad
    // is matched to its drum without playing a pattern first. Hearing them is the
    // app's business and recording them PatternRecorder's: an armed pattern records
    // whether or not it is on screen.
    void liveNoteOn(int pitch)  { labelsLiveNotes().noteOn(pitch); }
    void liveNoteOff(int pitch) { labelsLiveNotes().noteOff(pitch); }
    // Unlights every label row lit by the MIDI input. For when the editor stops
    // being the target, since the note-offs will then go elsewhere.
    void clearLiveNotes() { labelsLiveNotes().clear(); }
    // Instrument of the currently selected track's pattern (0 if none).
    int currentInstrumentId() const;
    // Flexible-bars recording has lengthened pattern `patId` and the playhead is at
    // `headBeat` in it. If that is the pattern on screen, keep the head in view as
    // the grid outgrows the viewport.
    void followGrowth(int patId, float headBeat);
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    // The param labels show each lane's MIDI-learn binding and live value.
    void setMidiLearn(const MidiLearnMap* m) { paramLabels.setMidiLearn(m); }
    // Skipped while not on screen: FLTK still marks a hidden widget's rectangle of
    // the window for repainting, and on Wayland that area flickers. Showing the
    // editor redraws it in full anyway.
    void redrawParamLabels() { if (paramLabels.visible_r()) paramLabels.redraw(); }
    void onTimelineChanged() override;
};

#endif
