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
#include <functional>
#include <optional>
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
    ObservablePattern* pattern             = nullptr;
    NoteAuditioner*    auditioner          = nullptr;
    int                lastSelectedTrack  = -1;
    int                lastSelectedLaneId = -1;
    int                lastPatId          = -1;
    int                colOffset         = 0;
    int                paramLaneOffset   = 0;
    int                baseColWidth      = 0;   // colWidth at zoom x1
    float              lastLengthBeats   = -1.0f;

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

    // Instrument of the currently selected track's pattern (0 if none).
    int currentInstrumentId() const;

    // ── Recording ────────────────────────────────────────────────────────────
    bool recArmed_ = false;
    // One undo entry for a whole take rather than one per note: UndoGroup keeps
    // notifying (so notes appear as they are played) but snapshots only once.
    // Held open across the take, released when it ends.
    std::optional<ObservableSong::UndoGroup> recUndo_;
    // Notes waiting on their note-off, so the length can be what was played.
    // Editors that record on the note-on (drums) never put anything here.
    struct RecordingNote { int pitch; float startBeat; int velocity; };
    std::vector<RecordingNote> recNotes_;

    // Write one finished note into the pattern. `lenBeats` is the played duration,
    // already wrapped and clamped to the pattern; editors that record on the
    // note-on ignore it. Default does nothing, so an editor that cannot record
    // silently accepts (and drops) anything sent its way.
    virtual void commitRecordedNote(int /*pitch*/, float /*startBeat*/,
                                    float /*lenBeats*/, int /*velocity*/) {}
    // True for editors whose notes have no length (the drum editor), which are
    // written as soon as the key goes down rather than when it comes up.
    virtual bool recordsOnNoteOn() const { return false; }
    // Pattern length in beats — the wrap point for a note held across the loop.
    int  recordPatternBeats() const { return gridNumCols(); }

    // onTimelineChanged skeleton hooks
    virtual void setGridPattern(int patId)    = 0;
    virtual void afterTimelineChanged(int /*patId*/) {}

    void setRowOffset(int offset);
    void setColOffset(int offset);
    void applyPatternLength(int patId);
    void updateParamScrollbar();
    void relayout();
    void layoutBody() override { relayout(); }
    void onWheelX(int d) override { setColOffset(colOffset + d); }
    void onWheelY(int d) override { setRowOffset(currentRowOffset() - d); }

    BasePatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                      int rowHeight, int colWidth, float snap, int lw);

public:
    ~BasePatternEditor();

    virtual void focusPattern() {}
    virtual void setSnap(float s) { paramGrid.setSnap(s); }
    // Beat subdivisions (1 = None): drawn as faint grid lines, independent of snapping.
    virtual void setDivisions(int d) { (void)d; }
    // Zoom factor (1/2/4): scales column width from its x1 base; note minimum
    // pixel widths are unaffected, so shorter notes remain creatable when zoomed.
    void setZoom(int factor);
    void setPatternPlayhead(ITransport* t, ObservablePattern* pat, int trackIndex);
    void setAuditioner(NoteAuditioner* a);

    // ── MIDI input ───────────────────────────────────────────────────────────
    // Editors that can take dictation from the MIDI input. Harmony cannot: its
    // rows are chord degrees and the reverse mapping from a played pitch is lossy,
    // so it auditions incoming notes but never records them.
    virtual bool canRecord() const { return false; }
    void setRecordArmed(bool on);
    bool recordArmed() const { return recArmed_; }

    // Fed from the MIDI input on the UI thread. Both always audition, so the
    // player hears the instrument; they additionally record when armed and the
    // transport is rolling.
    void midiNoteOn(int pitch, int velocity);
    void midiNoteOff(int pitch);
    // Releases everything still sounding and closes the open take, committing any
    // held notes with the length they reached. Called when the editor stops being
    // the MIDI target and when the transport stops.
    void releaseMidiNotes();
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    void onTimelineChanged() override;
};

#endif
