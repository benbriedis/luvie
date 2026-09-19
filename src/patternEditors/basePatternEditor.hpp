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
#include <chrono>
#include <functional>
#include <map>
#include <optional>
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

    // The shared front half of recording anything: checks the editor is armed and
    // the transport is rolling over a pattern, sets `beat` to where the playhead is
    // (pattern-relative), and opens the take on its first event. False means this
    // event is not recorded.
    bool beginRecordedEvent(float& beat);

    // ── Recording controller automation ──────────────────────────────────────
    // Values from a bound MIDI-learn control. They are buffered rather than written
    // one by one because every write is a notify(), which rebuilds the Sequencer
    // snapshot and, hosted, re-sends the whole timeline to the DSP — too much at
    // the rate a mod wheel sends. The buffer is written out from the playhead's
    // existing tick, at most every kParamFlushSecs, and at the end of the take.
    using Clock = std::chrono::steady_clock;
    static constexpr double kParamFlushSecs = 0.1;
    // Messages further apart than this belong to separate movements of the control.
    // Within one movement ("touch"), what was already on the lane between two
    // successive values is replaced; between movements it is left alone.
    static constexpr double kParamGestureGapSecs = 0.25;
    struct RecordingParam { std::string type; float beat; int value; bool gestureStart; };
    std::vector<RecordingParam> recParams_;
    struct ParamTouch { float beat = -1.0f; int value = -1; Clock::time_point at; };
    std::map<std::string, ParamTouch> recParamTouch_;   // per type, this take
    std::map<int, std::set<int>>      takeParamPoints_; // lane id -> points written this take
    Clock::time_point                 lastParamFlush_;
    // Write the buffered values into the pattern's lanes, creating a lane that is
    // missing, as one notify.
    void flushRecordedParams();
    // End of take: thin what this take wrote down to the points that shape it.
    void thinRecordedParams();

    // Write one finished note into the pattern. `lenBeats` is the played duration,
    // already clipped to the pattern; editors that record on the note-on ignore
    // it. Default does nothing, so an editor that cannot record silently accepts
    // (and drops) anything sent its way.
    virtual void commitRecordedNote(int /*pitch*/, float /*startBeat*/,
                                    float /*lenBeats*/, int /*velocity*/) {}
    // True for editors whose notes have no length (the drum editor), which are
    // written as soon as the key goes down rather than when it comes up.
    virtual bool recordsOnNoteOn() const { return false; }
    // Pattern length in beats — where a held note is clipped.
    int  recordPatternBeats() const { return gridNumCols(); }
    // The Snap quantum in beats, from the panel's Div setting, or 0 when Snap is
    // off. The same value the grids snap mouse edits to.
    float snapBeats_ = 0.0f;
    // `beat` rounded to the nearest division, or unchanged when Snap is off.
    float quantiseBeat(float beat) const;
    // A note just committed at `startBeat` was already heard live. If rounding put
    // it ahead of the playhead, playback would reach it moments later and sound it
    // again; ask (onSkipNoteOnce) for that one firing to be dropped.
    void skipLiveEcho(int pitch, float startBeat);
    // Turns the beats a key went down and came up at — both pattern-relative and
    // unrounded — into the note to write.
    //
    // With Snap on, start and end are each rounded to the nearest division, and a
    // note too short to survive that rounding is given one division rather than
    // disappearing. The raw beats, not the rounded ones, decide whether the
    // pattern wrapped under the key, because rounding can collapse a genuinely
    // short note onto the same answer as a key held for a whole pass.
    //
    // Either way the note is kept inside the pattern: if the pattern wrapped, or
    // the key outlasted what was left, it is clipped at the end rather than
    // wrapped round to the front. That is the rule the grid already applies to a
    // note dragged or pasted past the end, and in a loop the alternative — a note
    // reappearing at bar 1 with no key pressed — is not what was played.
    void recordedNoteSpan(float rawStart, float rawEnd,
                          float& startBeat, float& lenBeats) const;

    // ── Flexible bars ("Grow") ───────────────────────────────────────────────
    // With Grow armed the pattern loops as usual until the take's first note; from
    // there it gains a bar whenever the playhead nears the end, and the trailing
    // empty bars are trimmed off when the take finishes. Nothing here runs on the
    // RT thread: growth is an ordinary UI-thread edit, and the engine picks up the
    // new length through the snapshot it rebuilds for every other edit too.
    bool  growArmed_          = false;  // the toggle; session-only, never saved
    bool  growActive_         = false;  // this take is growing
    int   growInstId_         = 0;      // song block being grown; 0 on the loop path
    float growBaseBeats_      = 0.0f;   // pattern length at engagement — the trim floor
    float growBaseInstLength_ = 0.0f;   // block length at engagement — its trim floor

    // Beats in one of the PATTERN's own bars (its timeSigTop), or 0 with no pattern.
    // Distinct from Playhead::PatternPos::beatsPerBar, which counts pattern beats per
    // SONG bar; growth adds pattern bars but resizes blocks in song bars.
    float patternBarBeats() const;
    // The pattern's current length in beats, or 0 when there is no pattern. Growth
    // works from this rather than gridNumCols(), which is an int: a pattern whose
    // length is not a whole number of beats would otherwise have the grid and the
    // engine's modulo disagree about where it ends.
    float patternLengthBeats() const;
    // The lane the editor is showing, chosen exactly as patternIdForSelectedLane()
    // chooses it. Null when there is no track or it has no lanes.
    const Lane* selectedLane() const;
    // The song block of the displayed pattern under the playhead on the selected
    // lane, or nullptr — in Loop Mode, under a manual loop, or when the placement is
    // not the one the phase came from.
    const PatternInstance* growableInstance(float bars,
                                           const Playhead::PatternPos& pp) const;
    // Song Mode only: give the take a block to record into. A pattern can be edited
    // and recorded into with no placement at all — it simply runs alongside the song
    // — but a take is something you want to hear back, so the first note of one
    // places the pattern where it was played, on the pass the playhead is in. Does
    // nothing when a block is already there, and declines rather than overlap a
    // neighbour, since the song editor does not allow that.
    void ensureSongBlock();

    // Called on the first recorded note of a take. Re-phases the pattern so the pass
    // in progress becomes pass 0 and growth extends it rather than moving the loop
    // point under the playhead; then applies the invariant once.
    void engageGrow();
    // Per-tick, from the playhead's existing timer: keep at least one whole empty bar
    // beyond the bar the playhead is in, so growth is never on the critical path of a
    // tick interval or (in plugin mode) a trip through the host's worker thread.
    void growTick();
    // End of take: trim the bars nothing was recorded into and drop the state.
    void endGrow();

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
    virtual void setSnap(float s) { snapBeats_ = s; paramGrid.setSnap(s); sliceCtl.setSnap(s); }
    // Beat subdivisions (1 = None): drawn as faint grid lines, independent of snapping.
    virtual void setDivisions(int d) { (void)d; }
    // Horizontal zoom: `factor` scales the column width from its x1 base (so 0.2
    // through 4). Note minimum pixel widths are unaffected, so shorter notes
    // remain creatable when zoomed.
    void setZoom(float factor);
    void setPatternPlayhead(ITransport* t, ObservablePattern* pat, int trackIndex);
    void setAuditioner(NoteAuditioner* a);

    // ── MIDI input ───────────────────────────────────────────────────────────
    // Editors that can take dictation from the MIDI input. Harmony cannot: its
    // rows are chord degrees and the reverse mapping from a played pitch is lossy,
    // so it auditions incoming notes but never records them.
    virtual bool canRecord() const { return false; }
    void setRecordArmed(bool on);
    bool recordArmed() const { return recArmed_; }
    // Arm flexible bars. Only meaningful alongside Record, and like Record it is
    // session state — the project never stores it. Turning it off mid-take settles
    // the length there and then, the way disarming Record ends the take.
    void setGrowArmed(bool on);
    bool growArmed() const { return growArmed_; }

    // Fed from the MIDI input on the UI thread. Both always audition, so the
    // player hears the instrument; they additionally record when armed and the
    // transport is rolling.
    void midiNoteOn(int pitch, int velocity);
    void midiNoteOff(int pitch);
    // Whoever sequences playback: drop the one firing of (instrument, MIDI pitch)
    // within `tolBars` of song bar `bar`. See skipLiveEcho().
    std::function<void(int instrumentId, int pitch, double bar, double tolBars)> onSkipNoteOnce;
    // A bound MIDI-learn control moved. `value` is already in the lane's units.
    // Always sent through to the instrument; recorded into the pattern's lane of
    // that type when armed and rolling.
    void midiParam(const std::string& type, int value);
    // Releases everything still sounding and closes the open take, committing any
    // held notes with the length they reached. Called when the editor stops being
    // the MIDI target and when the transport stops.
    void releaseMidiNotes();
    // Unlights every label row lit by the MIDI input. For when the editor stops
    // being the target, since the note-offs will then go elsewhere.
    void clearLiveNotes() { labelsLiveNotes().clear(); }
    void setNoteLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamLabelsContextPopup(NoteLabelsContextPopup* popup);
    void setParamDotPopup(ParamDotPopup* p) { paramGrid.setParamDotPopup(p); }
    // The param labels show each lane's MIDI-learn binding and live value.
    void setMidiLearn(const MidiLearnMap* m) { paramLabels.setMidiLearn(m); }
    void redrawParamLabels() { paramLabels.redraw(); }
    void onTimelineChanged() override;
};

#endif
