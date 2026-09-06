// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef OBSERVABLE_SONG_HPP
#define OBSERVABLE_SONG_HPP

#include "itimelineobserver.hpp"
#include "timeline.hpp"
#include "patternNames.hpp"
#include <deque>
#include <string>
#include <vector>

class ObservablePattern;
class ObservableInstrument;
class ITransport;

class ObservableSong {
    friend class ObservablePattern;
    friend class ObservableInstrument;
public:
    ObservableSong(float initBpm, int initTop, int initBottom);

    void addObserver(ITimelineObserver* o);
    void removeObserver(ITimelineObserver* o);

    // The global tempo register's own channel; see IGlobalTempoObserver. Changes to it
    // reach these and nobody else, so a BPM edit never masquerades as a project edit.
    void addTempoObserver(IGlobalTempoObserver* o);
    void removeTempoObserver(IGlobalTempoObserver* o);

    const Timeline& get() const { return data; }

    // Optional transport, used only to keep the playhead musically anchored
    // across tempo edits (see reanchoringTempo). No-op until set.
    void setTransport(ITransport* t) { transport = t; }

    // Tempo, in beats per minute — a beat being the time signature's BeatUnit.
    // setBpm changes only the tempo the marker starts at, leaving its curve
    // alone; setBpmMarker sets the whole marker, ramp and all.
    void  setBpm(int bar, float bpm);
    void  setBpmMarker(int bar, float bpm, timeSettings::TempoCurve curve,
                       int lengthBars, float endBpm);
    void  removeBpm(int bar);
    void  moveBpmMarker(int fromBar, int toBar);
    // Move and/or resize a ramp. newEndBar is exclusive — the ramp ends flush
    // with the end of bar newEndBar-1. Both edges are clamped to the neighbouring
    // markers and to a minimum length of one bar; returns the bar the marker
    // ended up on, so a drag can follow it.
    int   resizeBpmRamp(int bar, int newBar, int newEndBar);

    const BpmMarker* bpmMarkerAt(int bar) const;   // exact match on bar, or null
    // The tempo in force immediately before `bar`, ignoring any marker sitting on
    // it — what a ramp starting there ramps away from. 0 when nothing precedes it.
    float inheritedBpmAt(int bar) const;
    // The bars a marker's ramp may be dragged between: no overlap with the
    // markers either side, and no longer than timeSettings::rampMaxBars.
    int   minRampStartBar(int bar) const;
    int   maxRampEndBar(int bar) const;

    // The tempo in force at a bar. bpmAtBar takes a fractional bar and steps
    // through a ramp beat by beat; bpmAt is the value at the bar's start.
    float bpmAtBar(float bar) const;
    float bpmAt(int bar) const { return bpmAtBar((float)bar); }

    // Tempo in crotchets per minute: the BPM scaled by the beat definition. This
    // is the tempo the timing math and the JACK/LV2 hosts speak in.
    float cpmAt(int bar) const;

    // Time signature (+ the beat definition stored with it)
    void setTimeSig(int bar, int top, int bottom,
                    timeSettings::BeatUnit beat = timeSettings::beatUnitDefault);
    void removeTimeSig(int bar);
    void moveTimeSigMarker(int fromBar, int toBar);
    void timeSigAt(int bar, int& top, int& bottom) const;
    timeSettings::BeatUnit beatAt(int bar) const;
    // Seconds one bar lasts at `bar`, from its time signature, beat and tempo.
    double secondsPerBarAt(int bar) const;

    // How many of a pattern's grid beats fit in one song bar at `bar`. Every
    // conversion between a pattern's beats and song bars must go through this:
    // a pattern's columns are units of ITS time signature, timed by ITS beat
    // definition, so they are not the song's beats unless the two signatures
    // agree. The patternId overload falls back to the song's numerator when the
    // pattern is gone. See timeSettings::patternBeatsPerBar.
    float patternBeatsPerBar(int bar, const Pattern& pat) const;
    float patternBeatsPerBar(int bar, int patternId) const;
    const Pattern* patternById(int patternId) const;

    // The clock's tempo map: one segment per tempo/time-signature breakpoint, plus
    // one per beat inside a linear ramp, each with the cumulative seconds at its
    // start. This is the single source of truth for bar<->seconds — the realtime
    // Sequencer takes a copy of it for its snapshot rather than rebuilding one.
    // Cached; invalidated by notify(). Both maps carry the global tempo (below);
    // this one additionally honours a Loop Mode hold, songTempoMap() never does.
    const std::vector<timeSettings::TempoSegment>& tempoMap() const;
    // What song playback is built from: the global tempo bounded by the song's own
    // markers, so a marker ahead still takes over when playback reaches it. The
    // snapshot endLoopMode() parks uses this, while the clock is still held.
    const std::vector<timeSettings::TempoSegment>& songTempoMap() const;

    // ── Global tempo ──────────────────────────────────────────────────────────
    //
    // One register: the tempo the clock is running at right now. Three things
    // write it, and nothing else does:
    //
    //   * the song's tempo markers, as playback reaches them;
    //   * the Loop Editor's BPM box (setGlobalBpm), which writes the register and
    //     never a marker — markers are how the *song* schedules tempo changes, and
    //     a jam is not a song edit;
    //   * the end of the run the typed value belongs to (readTempoFromMap), which
    //     re-reads it from the markers at the bar the playhead is left on, dropping
    //     whatever the user last typed. Two things end a run: a reposition the *user*
    //     made — a ruler seek or the rewind button — and the transport stopping. Both
    //     are events, never guesses; see the paragraph below.
    //
    // Nothing else may write it, and in particular nothing may infer a reposition
    // from the playhead having moved backwards. That was tried and it broke the
    // Loop -> Song hand-off in plugin mode: the UI settles the switch when the loop
    // clock crosses the bar line, a round trip before the engine actually lands it,
    // so the position drops afterwards and read as a reposition — throwing away the
    // very tempo the jam existed to carry.
    //
    // A value the user typed carries an anchor bar internally, and applies from there
    // up to the next tempo marker. That is how a register is handed to a clock with no
    // playback history to consult: in plugin mode the host gives the DSP a frame and
    // expects a bar back, so bar <-> seconds has to stay a pure function of position.
    //
    // THE ANCHOR RULE — the register may only ever recolour segments the song's own
    // markers already define; it may never introduce one of its own. So the anchor is
    // snapped back to the last tempo breakpoint at or before the bar it was set at (a
    // marker, one of a ramp's per-beat steps, or bar 0), which means the register
    // governs a whole marker-to-marker stretch rather than starting mid-stretch. The
    // guarantee that buys: every tempo change in the map coincides with a tempo
    // marker, whichever direction playback crosses it from and however it got there.
    //
    // It was not always so. The anchor used to sit at the playhead's own position,
    // relying on "playback only ever leaves it going forward, and every route back
    // re-reads the map" to keep that extra breakpoint invisible — and a song loop
    // wrapping back over it broke exactly that: an unmarked tempo change, at the bar
    // the user happened to leave Loop Mode on. Position-anchored patches to the map
    // are hidden markers by construction; do not reintroduce one.
    //
    // Nothing here is saved or undoable.
    //
    // Loop Mode *holds* the register (holdTempo): the value stops being bounded by
    // the markers ahead and the time signature is pinned with it, so a jam on the
    // free-running clock keeps one tempo and one meter however long it lasts. The
    // hold is released on the way back to Song Mode and the value survives, which
    // is what carries a tempo set while jamming into the song that follows — until
    // the transport stops, which is where that carried value runs out (below).
    void  setGlobalBpm(float bpm, float fromBar);
    float globalBpmAt(float bar) const;   // the tempo the clock will run at, there
    // The run the typed tempo belonged to has ended — the playhead was repositioned,
    // or playback stopped — so the register takes whatever the song's markers say at
    // `bar`. No-op while Loop Mode holds the tempo (the jam is still what is playing),
    // and no-op when nothing is overridden, so it costs nothing on the repositions
    // that happen constantly.
    void  readTempoFromMap(float bar);
    // Forget the register entirely. Only for opening a different project: a jam tempo
    // belongs to the session, and the song being loaded never saw it. Note this is not
    // loadTimeline()'s job — in plugin mode that call is also the routine UI -> DSP
    // sync for any edit at all.
    void  resetGlobalTempo();
    // Loop Mode pin/unpin. Nothing is notified: an observer fan-out here would have
    // the Sequencer resnapshot while the mode flag still says the old mode,
    // publishing song content over the running loops for a cycle. The mode change
    // that always follows does the rebuild instead.
    void  holdTempo(float atBar);
    void  releaseTempoHold();
    bool  tempoHeld()    const { return tempoHold; }
    // The bar Loop Mode froze on — where the held map stops, and the bar the Loop
    // panel's BPM box speaks for. Not the register's anchor: that is snapped back to
    // a marker (see the anchor rule), and truncating there instead would drop a
    // time-signature marker the playhead had already passed, pinning the wrong meter.
    float tempoHoldBar() const { return holdBar; }

    // Raw accessors + setter for mirroring the global tempo across a process
    // boundary (the LV2 UI ships these to the DSP in the loop atom). The setter is
    // deliberately not the re-anchoring one: the DSP has no ITransport to pin, and
    // its clock is the host's frame.
    bool  globalBpmSet()     const { return globalBpmOn; }
    float globalBpmValue()   const { return globalBpm; }
    float globalBpmFromBar() const { return globalBpmBar; }
    void  mirrorGlobalBpm(bool on, float bpm, float fromBar, bool held, float atHoldBar);

    // Time conversion — integrates over the tempo map above
    double barToSeconds(float bar) const;
    // The same against songTempoMap(), so it ignores a Loop Mode hold. What a resume
    // point must be measured in: playback lands on it after the hold has gone.
    double songBarToSeconds(float bar) const;
    float  secondsToBar(double secs) const;
    void   secondsToBarBeat(double secs, int& bar, int& beat) const;

    // Instrument management
    int  addInstrument(std::string name, bool isDrum = false);
    void renameInstrument(int instrId, std::string name);
    void removeInstrument(int instrId);

    // Track management
    int  addTrack(int instrumentId = 0, int patternId = 0, int atIndex = -1);
    void removeTrack(int trackId);
    void setTrackSolo(int trackId, bool solo);
    void setTrackMute(int trackId, bool mute);
    bool isTrackPlaying(int trackId) const;
    void selectTrack(int index);
    void selectLane(int trackIndex, int laneId);
    int  addLane(int trackId);
    int  addPianorollLane(int trackId);
    int  cloneLane(int trackId, int laneId);
    void removeLane(int trackId, int laneId);
    void setStackedLanes(int trackId, bool stacked);
    int  trackIndexForId(int trackId) const;
    int  trackIndexForLaneId(int laneId) const;
    int  trackIdForLaneId(int laneId) const;
    int  instrumentIdForTrack(int trackId) const;

    // Which context-menu items apply to a given track (greyed-out state).
    struct TrackMenuFlags {
        bool canOpenPattern = false;
        bool canRemoveLane  = false;
        bool isDrumTrack    = false;
    };
    TrackMenuFlags trackMenuFlags(int trackId) const;
    // rowOrder insertion index for a new param lane belonging to this track.
    int  paramLaneInsertIndex(int trackId) const;
    void moveRow(int fromRowIdx, int toGapIdx);
    void moveTrack(int trackId, int insertBeforeTrackId);
    // Loop editor instrument ordering (independent of the song editor's rowOrder).
    // loopOrder holds track IDs; insertBeforeTrackId < 0 appends at the end.
    void moveLoopInstrument(int trackId, int insertBeforeTrackId);
    // Loop editor pattern (lane) ordering, independent of rowOrder per track.
    // True when laneId's pattern drum-ness matches destTrackId's instrument.
    bool canMoveLaneToTrack(int laneId, int destTrackId) const;
    // Reorder within an instrument (destTrackId == lane's track) or move to another;
    // beforeLaneId < 0 appends. Returns false if the move is forbidden/not possible.
    bool moveLoopPattern(int laneId, int destTrackId, int beforeLaneId);
    void rebuildInstrumentHeaders();
    // Predict which track a lane/param row at `from` would belong to if dropped at
    // `toGap`, without mutating. Returns -1 if it cannot be determined. Mirrors the
    // destination resolution inside moveRow so callers can validate a drop first.
    int  predictRowDropTrack(int from, int toGap) const;

    // Pattern lifecycle helpers (not note editing)
    int nextTrackNumberForType(PatternType type) const;
    void removeTrackAndPattern(int trackId);
    const Pattern*         patternForInstance(int instanceId) const;
    const PatternInstance* instanceById(int instanceId) const;
    int                    laneIdForInstance(int instanceId) const;

    // Default instrument IDs assigned to newly created patterns (0 = none).
    int defaultInstrumentId     = 0;
    int defaultDrumInstrumentId = 0;

    // Set the display name of a pattern. Rejected (no change) if the name is
    // blank or duplicates another pattern; all naming is funnelled through
    // PatternNames to keep names unique.
    void setPatternName(int patId, std::string name);

    // Would renaming pattern patId to `name` collide with another pattern?
    // Used by the editors for live duplicate feedback while typing.
    bool patternNameCollides(int patId, const std::string& name) const {
        return patternNames.collides(name, patId);
    }

    // Pattern instance management (instances identified by stable id)
    void addPattern(int trackIndex, float startBar, float length, float patternBeats = 0.0f);
    void removePattern(int instanceId);
    void movePattern(int instanceId, int newLaneId, float newStartBar);
    void resizePattern(int instanceId, float newLength);
    void resizePatternLeft(int instanceId, float newStartBar, float newLength, float newStartOffset);
    void setPatternStartOffset(int instanceId, float startOffset);
    // Returns the new instance's id, or 0 if no such lane. startOffset is carried
    // through so a copy of a left-resized instance keeps playing from the same
    // beat of its pattern.
    int  placePattern(int laneId, int patternId, float startBar, float length,
                      float startOffset = 0.0f);

    // Song-level param lane management. Each lane belongs to one instrument and
    // routes only to that instrument's port; uniqueness is per (type, instrument).
    bool hasParamLane(const std::string& type, int instrumentId) const;
    int  addParamLane(const std::string& type, int instrumentId, int atIndex = -1);
    void removeParamLane(int laneId);
    int  instrumentIdForParamLane(int laneId) const;
    int  addParamPoint(int laneId, float beat, int value);
    void removeParamPoint(int pointId);
    void moveParamPoint(int pointId, float beat, int value);

    // Build a flat Note list for grid consumption (row = track index)
    std::vector<Note> buildNotes() const;

    // Replace the entire timeline at once and notify observers.
    void loadTimeline(const Timeline& tl);

    // ── Undo/redo ────────────────────────────────────────────────────────────
    // Whole-timeline snapshots: Timeline is a plain aggregate of vectors, so a
    // copy is the whole story — no command objects, no inverse operations, and
    // no mutation needs to know undo exists. The snapshot is taken in notify(),
    // which every mutation already funnels through.
    void undo();
    void redo();
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    // Make the current state the baseline. Startup builds the default song
    // through the ordinary mutators, so without this the user could undo their
    // way back past an empty song before touching anything.
    void clearHistory() { undoStack.clear(); redoStack.clear(); lastCommitted = data; }

    // Coalesces a burst of mutations into one notification and one undo entry.
    // Re-entrant: only the outermost guard notifies. Wrap any loop of single-item
    // mutations in this — a notify() costs a full Sequencer snapshot rebuild and,
    // in plugin mode, a complete timeline serialization to the DSP.
    class Batch {
        ObservableSong* song;
    public:
        explicit Batch(ObservableSong* s) : song(s) { song->batchDepth++; }
        ~Batch() {
            if (--song->batchDepth == 0 && song->batchDirty) {
                song->batchDirty = false;
                song->notify();
            }
        }
        Batch(const Batch&)            = delete;
        Batch& operator=(const Batch&) = delete;
    };

    // Folds a run of mutations into ONE undo entry while still notifying after
    // each one. Batch cannot serve here: a drag-paint stroke has to redraw as
    // it goes, so the notifications must flow, but the whole stroke should be a
    // single ctrl-Z. Held open across a gesture, not a function call.
    class UndoGroup {
        ObservableSong* song;
    public:
        explicit UndoGroup(ObservableSong* s) : song(s) {
            if (song->undoGroupDepth++ == 0) song->undoGroupSnapped = false;
        }
        ~UndoGroup() { --song->undoGroupDepth; }
        UndoGroup(const UndoGroup&)            = delete;
        UndoGroup& operator=(const UndoGroup&) = delete;
    };

    // Notify for a change that is UI state rather than song content — the
    // selected track, a zoom level. Keeps the undo mirror in step so the NEXT
    // real edit still snapshots correctly, but pushes no undo entry: undoing
    // through a stack of "I clicked a different track" would be miserable.
    void notifyViewState();

private:
    void buildTempoMap() const;
    mutable std::vector<timeSettings::TempoSegment> tempoMapCache;
    mutable bool tempoMapDirty = true;

    // The global tempo register; see the block above. globalBpmOn says whether the
    // user's own value is currently in force: false whenever the register is simply
    // what the markers say, which is the state every reposition returns it to, and
    // then both maps are just the song's markers. globalBpm is kept meaningful in
    // both states so the LV2 mirror always ships a real tempo. The held map is
    // cached alongside the song one and both are rebuilt whenever the tempo or the
    // song's markers change.
    bool  globalBpmOn  = false;
    float globalBpm    = 0.0f;
    float globalBpmBar = 0.0f;
    bool  tempoHold    = false;   // Loop Mode: unbounded, and the meter pinned too
    float holdBar      = 0.0f;    // the bar Loop Mode froze on; where the held map ends
    mutable std::vector<timeSettings::TempoSegment> heldMapCache;
    mutable bool heldMapDirty = true;
    // Stand-in for "no marker ahead": a bar count no song will reach.
    static constexpr double tempoForever = 1.0e9;
    // First tempo breakpoint after `bar`, or tempoForever if there is none.
    double nextTempoBreakAfter(double bar) const;
    // The last tempo breakpoint at or before `bar`: a marker's bar, one of a ramp's
    // per-beat steps, or 0. Every one of them is already a segment start in the map,
    // which is the point — see the anchor rule above.
    double tempoBreakAtOrBefore(double bar) const;
    // Is the register the tempo in force at `bar`? The one test behind globalBpmAt()
    // and behind the anchor rule: a write that lands inside the stretch the register
    // already governs changes its value, never where it applies.
    bool   overrideCovers(float bar) const;

    Timeline data;
    // Funnel for all pattern-name creation/changes; binds to data.patterns.
    PatternNames patternNames{data};
    std::vector<ITimelineObserver*>   observers;
    std::vector<IGlobalTempoObserver*> tempoObservers;
    ITransport* transport = nullptr;
    int nextId = 1;

    // Run a tempo-map mutation while keeping the transport anchored to its
    // current musical position. The transport stores its play anchor in seconds
    // (derived from the old tempo map), so any change to that map would remap
    // the same elapsed wall-clock time to a different bar and the playhead would
    // scrub. We sample the position under the old map, apply the change, then
    // re-seek so the playhead stays put and merely advances at the new rate.
    template <class F>
    void reanchoringTempo(F&& mutate);

    // True if patId is referenced by any lane's editor pattern or any placed instance.
    bool patternStillReferenced(int patId) const;

    void notify();
    // The observer fan-out on its own, with no undo bookkeeping. notify() and
    // the undo machinery both go through it.
    void fanout();
    // The global tempo's fan-out: invalidates the cached maps and tells the tempo
    // observers, and does nothing else. No undo entry, and no timeline notification —
    // the register is not song content.
    void tempoFanout();
    // Swap in a snapshot without recording one. Deliberately NOT loadTimeline():
    // that re-runs file migration and recomputes nextId from the data, which
    // would walk the counter backwards on every undo and let a later edit
    // reissue an id a live view still holds.
    void restoreSnapshot(const Timeline& tl);

    std::deque<Timeline> undoStack, redoStack;
    // The state as of the previous notify. Because it only advances when a
    // notify actually fires, it is always the pre-change state — including
    // across a Batch, which is what makes a batch one undo entry.
    Timeline lastCommitted;
    int      batchDepth = 0;
    bool     batchDirty = false;
    int      undoGroupDepth   = 0;
    bool     undoGroupSnapped = false;
    static constexpr int undoDepth = 100;

    // Drop stale track IDs from loopOrder and append any missing tracks.
    void reconcileLoopOrder();
    // Per track: drop stale lane IDs from loopLanes and append any missing lanes.
    void reconcileLoopLanes();
    void sortBpms();
    void sortTimeSigs();
    void removeParamLanesForInstrument(int instrumentId);
};

#endif
