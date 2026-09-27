// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PATTERN_RECORDER_HPP
#define PATTERN_RECORDER_HPP

#include "itransport.hpp"
#include "observablePattern.hpp"
#include "playhead.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class LoopManager;

// Takes dictation from the MIDI input into one pattern. Pianoroll and drum patterns
// only — harmony rows are chord degrees and the reverse mapping from a played pitch
// is lossy.
//
// A recorder belongs to its pattern, not to the editor, so an armed pattern keeps
// recording after the editor has moved on to show something else, and several can
// record at once. It follows its pattern with a Playhead of its own, which reads the
// same phase the editor's does (LoopManager, including the free anchors), so a note
// lands exactly where the head is drawn whenever the pattern is on screen.
//
// Nothing here runs on the RT thread: every write is an ordinary UI-thread edit.
// Record and Grow are session state — the project never stores them.
class PatternRecorder {
public:
    PatternRecorder(ObservablePattern* pattern, ITransport* transport, LoopManager* loopMgr,
                    int patId, int laneId);
    ~PatternRecorder();
    PatternRecorder(const PatternRecorder&)            = delete;
    PatternRecorder& operator=(const PatternRecorder&) = delete;

    int  patternId() const { return patId_; }
    // The lane the pattern was armed from. Only a preference: the take's block goes
    // on the lane that shows this pattern (recordLane). Left alone mid-take.
    void setLaneId(int laneId) { if (!recUndo_) laneId_ = laneId; }
    // False once the pattern has gone (deleted, or undone out of existence).
    bool valid() const;
    // The pattern's instrument: what hears the notes, and whose input feeds them.
    int  instrumentId() const;

    // Disarming ends the take: held notes are committed with the length they
    // reached, and the undo group closes so the next take is its own entry.
    void setRecordArmed(bool on);
    bool recordArmed() const { return recArmed_; }
    // Flexible bars. Only meaningful alongside Record. Turning it off mid-take
    // settles the length there and then, the way disarming Record ends the take.
    void setGrowArmed(bool on);
    bool growArmed() const { return growArmed_; }
    void setLoopMode(bool loop) { playhead.setLoopActive(loop); }
    // The song bar the transport was last started from, or < 0 when it is not
    // rolling. A take in Song Mode keeps everything from there (ensureSongBlock).
    void setRunStart(float bar) { runStartBar_ = bar; }

    // Fed from the MIDI input on the UI thread. They record when armed and the
    // transport is rolling; hearing the note is the caller's business.
    void noteOn(int pitch, int velocity);
    void noteOff(int pitch);
    // A bound MIDI-learn control moved. `value` is already in the lane's units.
    void param(const std::string& type, int value);
    // Closes the open take, committing any held notes with the length they reached.
    void endTake();

    // Whoever sequences playback: drop the one firing of (instrument, MIDI pitch)
    // within `tolBars` of song bar `bar`. See skipLiveEcho().
    std::function<void(int instrumentId, int pitch, double bar, double tolBars)> onSkipNoteOnce;
    // Flexible bars just lengthened the pattern; `headBeat` is where the playhead is
    // in it. So an editor showing the pattern can keep the head on screen.
    std::function<void(int patId, float headBeat)> onGrew;

private:
    ObservablePattern* pattern = nullptr;
    int                patId_  = 0;
    int                laneId_ = 0;
    // Follows patId_ whatever the editor is showing (Playhead::setFixedPattern).
    // Never drawn; it is here for the phase, and for its timer (see the ctor).
    Playhead           playhead{1, 1};

    const Pattern* pat() const;
    bool recordsOnNoteOn() const;   // drums: a hit has no length, so it is written at once
    void commitNote(int pitch, float startBeat, float lenBeats, int velocity);
    // Pattern length in beats — where a held note is clipped. Whole beats, as the
    // editor's grid has; the playhead is kept on the same count.
    int   recordPatternBeats() const;
    // Where the transport is in the pattern, in beats, or -1 when it is nowhere.
    float headBeat();

    // ── Recording ────────────────────────────────────────────────────────────
    bool recArmed_ = false;
    // One undo entry for a whole take rather than one per note: UndoGroup keeps
    // notifying (so notes appear as they are played) but snapshots only once.
    // Held open across the take, released when it ends. Nests with the groups of
    // other recorders taking at the same time.
    std::optional<ObservableSong::UndoGroup> recUndo_;
    // Notes waiting on their note-off, so the length can be what was played.
    // Drum patterns record on the note-on and never put anything here.
    struct RecordingNote { int pitch; float startBeat; int velocity; };
    std::vector<RecordingNote> recNotes_;

    // The shared front half of recording anything: checks the recorder is armed and
    // the transport is rolling, sets `beat` to where the playhead is (pattern-
    // relative), and opens the take on its first event. False means this event is
    // not recorded.
    bool beginRecordedEvent(float& beat);

    // ── Recording controller automation ──────────────────────────────────────
    // Values from a bound MIDI-learn control. They are buffered rather than written
    // one by one because every write is a notify(), which rebuilds the Sequencer
    // snapshot and, hosted, re-sends the whole timeline to the DSP — too much at
    // the rate a mod wheel sends. The buffer is written out from the playhead's
    // tick, at most every kParamFlushSecs, and at the end of the take.
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

    // The pattern's Snap quantum in beats, or 0 when its Snap is off.
    float snapBeats() const;
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
    // empty bars are trimmed off when the take finishes. Growth is an ordinary
    // UI-thread edit, and the engine picks up the new length through the snapshot
    // it rebuilds for every other edit too.
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
    // works from this rather than recordPatternBeats(), which is an int: a pattern
    // whose length is not a whole number of beats would otherwise have the grid and
    // the engine's modulo disagree about where it ends.
    float patternLengthBeats() const;
    // The lane showing this pattern — the only lane a block of it may sit in —
    // preferring the one it was armed from. Null when there is none.
    const Lane* recordLane() const;
    // The song block of the pattern under the playhead on the record lane, or
    // nullptr — in Loop Mode, under a manual loop, or when the placement is not the
    // one the phase came from.
    const PatternInstance* growableInstance(float bars,
                                           const Playhead::PatternPos& pp) const;
    // Song Mode only: give the take a block to record into. A pattern can be edited
    // and recorded into with no placement at all — it simply runs alongside the song
    // — but a take is something you want to hear back, so the first note of one
    // places the pattern in the song. The block starts where the transport was
    // started, so nothing played since is cut off, and runs to the end of the pass
    // the playhead is in (with Grow, the pattern is lengthened to one pass that
    // reaches it). Each recording pattern places its own. Does nothing when a block
    // is already under the playhead, and fits between neighbours rather than
    // overlapping them, since the song editor does not allow that.
    void ensureSongBlock();
    float runStartBar_     = -1.0f;
    bool  placedForTake_   = false;  // ensureSongBlock placed this take's block
    float placedBaseBeats_ = 0.0f;   // the pattern's length before it did

    // Called on the first recorded note of a take. Re-phases the pattern so the pass
    // in progress becomes pass 0 and growth extends it rather than moving the loop
    // point under the playhead; then applies the invariant once.
    void engageGrow();
    // Per-tick, from the playhead's timer: keep at least one whole empty bar beyond
    // the bar the playhead is in, so growth is never on the critical path of a tick
    // interval or (in plugin mode) a trip through the host's worker thread.
    void growTick();
    // End of take: trim the bars nothing was recorded into and drop the state.
    void endGrow();
};

// Every pattern with Record or Grow armed, each with its own recorder. A recorder
// exists while either toggle is on and is dropped when both are off, so the arm
// states outlive the editor showing them without any separate bookkeeping.
class PatternRecorders {
public:
    ~PatternRecorders();

    void setContext(ObservablePattern* pattern, ITransport* transport, LoopManager* loopMgr);

    bool recordArmed(int patId) const;
    bool growArmed(int patId) const;
    bool anyRecordArmed() const;
    // `laneId` is the lane the pattern is being armed from (see PatternRecorder).
    void setRecordArmed(int patId, int laneId, bool on);
    void setGrowArmed(int patId, int laneId, bool on);
    // Transport stopped: every take ends and Record is disarmed everywhere, since
    // the next take is something the user asks for again, not something play
    // resumes. Grow stays armed. True if anything was disarmed.
    bool disarmAllRecord();
    // Drop everything, open takes included — a different project has been loaded.
    void clear();

    // Called with each record-armed recorder whose pattern still exists.
    void forEachRecording(const std::function<void(PatternRecorder&)>& fn);

    void setLoopMode(bool loop);
    // The transport started from song bar `bar`; < 0 when it stops.
    void setRunStart(float bar);

    std::function<void(int instrumentId, int pitch, double bar, double tolBars)> onSkipNoteOnce;
    std::function<void(int patId, float headBeat)> onGrew;

private:
    ObservablePattern* pattern   = nullptr;
    ITransport*        transport = nullptr;
    LoopManager*       loopMgr   = nullptr;
    bool               loopMode  = false;
    float              runStart  = -1.0f;
    std::vector<std::unique_ptr<PatternRecorder>> recs;

    PatternRecorder* find(int patId) const;
    PatternRecorder& obtain(int patId, int laneId);
    // Drop the recorder when neither toggle is on any more.
    void release(int patId);
};

#endif
