// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include "itimelineobserver.hpp"
#include "observableSong.hpp"
#include "loopManager.hpp"
#include "timeline.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// A MIDI note-off has status nibble 0x80. Backends use this to end a note one
// frame early (half-open interval), so a flush same-pitch note re-attacks.
inline bool isNoteOff(const uint8_t* data, int len)
{
    return len >= 1 && (data[0] & 0xF0) == 0x80;
}

/*
 * Sequencer — backend-agnostic MIDI sequencing core shared by every output path.
 *
 * It owns the timeline "snapshot" (a lock-free-ish RT-readable copy of everything
 * needed to generate MIDI) and the firing logic that, given a bar window
 * [prevBars, curBars), produces note-on/off and CC/pitch-bend messages. It does
 * NOT know how those messages reach the world: each message is handed to the
 * pure-virtual emit() with a *bar* position, and the concrete backend converts
 * that bar to its own frame/time domain and writes it out.
 *
 *   - JackTransport drives this from the JACK transport + its own MIDI ports.
 *   - The LV2 DSP drives it from the host's time:Position and forges the messages
 *     into an atom MIDI output port.
 *
 * Thread model (identical to the old JackTransport):
 *   - All setters (setTimeline/setInstruments/...) run on the owner thread that is
 *     the single writer of the snapshot (UI thread standalone; LV2 worker thread in
 *     the plugin).
 *   - renderCycle() runs on the real-time thread. It try_locks snapMutex once for
 *     the whole cycle; on failure it does nothing and returns false so the caller
 *     leaves wasPlaying untouched (the missed stop/jump is handled next cycle). It
 *     also owns the song-loop wrap, splitting the cycle at the loop seam, and the
 *     Loop -> Song hand-off.
 *   - emit() is called only from renderCycle() (RT thread) and must not allocate.
 */
class Sequencer : public ITimelineObserver, public ILoopObserver,
                  public IGlobalTempoObserver {
public:
    Sequencer();
    ~Sequencer() override;

    // Instrument routing: maps instrument ID -> (portName, 1-based MIDI channel).
    struct InstrumentRouting {
        int         instrumentId;
        std::string portName;
        int         midiChannel;          // 1-based
        int         programNumber = -1;   // -1 = not set; 0-127 = MIDI program
        int         bankMsb       = -1;   // -1 = not set; 0-127 = CC#0 value
        int         bankLsb       = -1;   // -1 = not set; 0-127 = CC#32 value
    };

    void setTimeline(ObservableSong* tl);
    void setLoopManager(LoopManager* loopMgr);
    void setInstruments(const std::vector<InstrumentRouting>& routings);
    void setLoopMode(bool loopMode);

    // Song-loop region (the song editor's Start/End markers), in absolute song
    // bars with endBar exclusive. When enabled and playing, renderCycle() wraps
    // musical playback from endBar back to startBar *within the RT cycle* — the
    // window that straddles the seam is split and rendered in two, notes still
    // held across the boundary are released at the seam frame, and no transport
    // relocate is involved. Owner thread; read lock-free on the RT thread.
    void setSongLoop(bool enabled, float startBar, float endBar);

    // Arm the Loop -> Song hand-off: build the song-mode snapshot now, and hand the
    // RT thread both it and `resumeBar`. The RT thread decides *when* — it applies the
    // switch at the first frame whose intra-bar phase matches resumeBar's, splitting
    // the cycle there exactly as the loop seam is split. That is the whole point: the
    // message may take tens of milliseconds to arrive (UI -> host -> LV2 worker), and
    // applying it on arrival would snap playback backwards by that latency. Choosing
    // the moment on the RT thread makes the hand-off beat-exact regardless.
    //
    // The loops keep playing from the live snapshot until the seam; at it, notes still
    // held are released, the pre-built snapshot is swapped in and the song's timeline
    // is slid in time so the position becomes exactly resumeBar. That snapshot carries
    // the song's own tempo map — Loop Mode runs on a frozen one — so the slide is
    // measured against the map playback is about to use, not the one it is leaving.
    // The backend clock is never relocated and controllers are never reset. Owner
    // thread.
    void endLoopMode(float resumeBar);

    // Arm a Loop-mode scene change: build the scene LoopManager::pendingPatterns()
    // describes now, and let the RT thread swap it in at the next bar line. Same
    // reason as endLoopMode() for choosing the moment there rather than here — the
    // message may take tens of milliseconds to arrive — but far cheaper: the mode,
    // the tempo map and the clock offset are all untouched, and nothing is reset, so
    // patterns in both scenes sustain across the seam with their phase intact.
    // No-op outside Loop mode. Owner thread.
    void armScene(float atBar);

    // Arm the Song -> Loop hand-off, the mirror of endLoopMode(): the song plays out
    // to the bar line at `atBar` and the loops come in there, so entering Loop mode
    // mid-bar no longer cuts the song off under the mouse.
    //
    // Precondition: the caller has already applied ObservableSong::holdTempo(atBar).
    // This flips loopMode before building so the parked snapshot carries the held map
    // the loops will run on — get that order wrong and it parks the song's map. The
    // backend clock is never relocated. No-op if already in Loop mode. Owner thread.
    void beginLoopMode(float atBar);

    // Drop a switch that is armed but has not landed yet — the user clicked back
    // into Loop mode, seeked, or chose a different scene. setLoopMode(), endLoopMode()
    // and armScene() all call it, so arming anything supersedes whatever was pending.
    // Owner thread; briefly blocks on snapMutex.
    void cancelPending();

    // Suspend snapshot rebuilds while a multi-step change is applied (the mode and
    // the active loop set together), so the RT thread sees one commit rather than an
    // intermediate snapshot at the old position. The caller must finish with
    // rebuildSnapshot() or endLoopMode(), which commit regardless. Owner thread.
    void suspendRebuilds(bool on) { rebuildsSuspended = on; }

    // The musical bar the engine currently reads `secs` as, under the live snapshot
    // and clock offset. Owner-thread counterpart to barInfo(): it blocks on the
    // snapshot lock rather than skipping the way the RT path does, because a
    // re-anchor has to have an answer.
    double barAtSeconds(double secs);

    // Publish a new tempo table and clock offset as one commit, leaving the rest of
    // the snapshot in place. Two reasons it is not a rebuildSnapshot():
    //   * a tempo change moves no note, track or routing data, so copying the whole
    //     song for one would be waste — and a dragged BPM spinner fires this many
    //     times a second, holding the lock long enough for the RT thread's try_lock
    //     to start failing, which is a skipped cycle and an audible dropout;
    //   * the map and the offset have to land together. A cycle that saw the new map
    //     with the old offset — or the reverse — would read the same frame as a
    //     different bar and playback would jump. renderCycle() loads the offset after
    //     taking the snapshot lock, so storing it here is atomic against a cycle.
    // Owner thread.
    void retempoSnapshot(std::vector<timeSettings::TempoSegment> segs,
                         double newSecsOffset);

    // Drop the one firing of (instrument, pitch) within tolBars of musical bar `bar`
    // — a recorded note that was already heard live and was quantised just ahead of
    // the playhead (see ITransport::skipNoteOnce). An entry that is never matched
    // expires once the window has passed it, and any reset clears them all. Owner
    // thread; briefly blocks on snapMutex.
    void skipNoteOnce(int instrumentId, int midiPitch, double bar, double tolBars);

    // ITimelineObserver / ILoopObserver / IGlobalTempoObserver
    void onTimelineChanged()       override { rebuildSnapshot(); }
    void onLoopsChanged() override { rebuildSnapshot(); }
    void onGlobalTempoChanged()    override;

    // The tempo table this engine's snapshot is built from, chosen by the mode the
    // way buildSnapshot() chooses it: Loop Mode runs on the held map, song content on
    // the marker-bounded one.
    const std::vector<timeSettings::TempoSegment>& activeTempoMap() const;

protected:
    // ── Snapshot (RT-readable copy of the timeline) ───────────────────────────
    // The tempo table is a straight copy of one of ObservableSong's maps — the
    // held tempoMap() for loop content, the marker-bounded songTempoMap() for song
    // (see buildSnapshot). See timeSettings::TempoSegment for what it holds and why
    // a linear tempo ramp needs no special case here.
    using TimeSegment = timeSettings::TempoSegment;
    struct NoteSnap    { int midiPitch; float beat; float length; float velocity; };
    struct InstanceSnap {
        float startBar;
        float length;
        float startOffset;
        float beatsPerBar;
        float patternBeats;
        bool  loop = false;      // loop-mode instance: anchor is phase only, plays forever
        std::string portName;    // routing key; meaning is backend-defined
        int   midiChannel = 0;   // 0-based MIDI channel
        std::vector<NoteSnap> notes;
    };
    struct TrackSnap { std::vector<InstanceSnap> instances; };
    struct ParamEventSnap { float beat; int value; };  // beat: within-pattern or bar
    struct ParamInstSnap {
        float startBar;
        float length;
        float startOffset;
        float beatsPerBar;
        float patternBeats;      // 0 = song-level (no loop); >0 = pattern length (beats)
        bool  loop = false;
        std::string portName;
        int   midiChannel;       // 0-based
        int   priority;          // 0 = song-level; trackIdx+1 for pattern lanes
        int   ccNumber;          // 1,7,10,11; -1 = pitch bend
        std::vector<ParamEventSnap> events;
    };
    struct Snapshot {
        std::vector<TimeSegment>   segs;
        std::vector<TrackSnap>     tracks;
        std::vector<ParamInstSnap> paramInsts;
        // Which branch of the builder produced this content. Carried here rather
        // than read from the owner-thread `loopMode` so the RT thread sees the mode
        // and the notes it belongs to swap together.
        bool                       loopMode = false;
    };

    mutable std::mutex snapMutex;
    Snapshot           snap;
    // The snapshot a pending hand-off will switch to, built ahead of the switch so the
    // RT thread can adopt it mid-cycle without allocating. While a hand-off is armed,
    // rebuildSnapshot() refreshes this one instead of `snap` — the loops are still the
    // thing playing. Guarded by snapMutex.
    Snapshot           pendingSnap;

    // The data model. Set via setTimeline(); read by backends (e.g. for UI-thread
    // position/seek conversions). Owned by the caller.
    ObservableSong* timeline = nullptr;

    void   setSampleRate(double sr) { sampleRateHz = sr; }
    bool   songLoopActive() const { return songLoopOn.load(std::memory_order_relaxed); }

    // Seconds subtracted from the backend clock before it is read against the tempo
    // map, so the playhead can be repositioned without relocating that clock: the
    // song's timeline is simply slid along in time. Used by the tempo re-anchor and
    // by the Loop -> Song hand-off; renderCycle() clears it on any clock jump (a seek
    // of ours, or a host relocate), which re-establishes the identity mapping.
    //
    // It is a *time* shift and not a bar shift for a reason. A bar shift moves only
    // the label: the map would still be read at the raw clock bar, so every tempo and
    // time-signature marker ahead would take effect early by however many bars the
    // offset covers. Shifting time instead delays the whole song timeline, leaving
    // each marker exactly where the user put it.
    //
    // Owner thread writes, RT thread reads; lock-free.
    void   setSecsOffset(double off) { secsOffset.store(off, std::memory_order_relaxed); }
    double secsOffsetSecs() const    { return secsOffset.load(std::memory_order_relaxed); }
    // The (possibly wrapped) musical bar playback is currently at — for the UI
    // playhead. Published by renderCycle() every cycle; lock-free.
    float  loopedPosition() const { return loopedBar.load(std::memory_order_relaxed); }

    void   rebuildSnapshot();
    // Exchange `snap` and `pendingSnap`. RT-safe: swaps each vector member (a pointer
    // exchange), never a move-assignment, which would free the old snapshot's memory
    // on the audio thread. snapMutex must be held.
    void   swapSnapshots();
    // Both directions stay in double. timeSettings' maps are double throughout, and
    // narrowing here used to cost real samples: the per-cycle advance is computed as
    // the difference of two absolute bar positions, so a float's rounding at bar N
    // lands directly on the musical position and accumulates over the ~700 cycles a
    // loop iteration takes. Measured at 34 samples of error on an 8-bar loop.
    double snapBarToSeconds(double bar)  const;   // reads snap.segs; hold snapMutex
    double snapSecondsToBar(double secs) const;

    // ── RT firing ─────────────────────────────────────────────────────────────
    // Render one RT cycle spanning [cycleStartSecs, cycleEndSecs) on Luvie's
    // timeline; the bar offset above is applied on top. Acquires snapMutex once for
    // the whole cycle, adopts any pending hand-off, converts the clock window to a
    // musical window, and — when the song loop is armed and playing — splits it at
    // the loop seam so playback wraps seamlessly. Returns true if the snapshot was
    // readable and at least one sub-window rendered; false means the caller should
    // leave wasPlaying unchanged (missed stop/jump retried next cycle). Calls emit()
    // for every message produced.
    //
    // Both ends are passed in rather than start + duration, and each backend must
    // derive them from its integer frame counter with the *same* expression
    // (frame/sr and (frame+nframes)/sr). Otherwise one cycle's end and the next
    // cycle's start differ by an ULP, and the windows — which are half-open — leave
    // a gap that silently swallows any event landing exactly on a buffer boundary
    // (or overlap, firing it twice).
    bool renderCycle(bool nowPlaying, bool jumped,
                     double cycleStartSecs, double cycleEndSecs);

    // Frame offset within the current cycle for an emitted musical `bar`, honouring
    // the active sub-segment's time base (so events past a loop wrap land at the
    // right frame). Call from emit(); result is not clamped to [0, nframes).
    long segFrameOffset(double bar) const;

    bool wasPlaying = false;   // owner: updated by the backend after renderCycle

    // Backend output hook: emit one 1-3 byte MIDI message for `port` at bar
    // position `bar` within the current cycle. Called on the RT thread; must not
    // allocate. The backend converts `bar` to its frame/time domain.
    virtual void emit(const std::string& port, double bar,
                      const uint8_t* data, int len) = 0;

private:
    // What to emit before a sub-window's own events.
    //   None — nothing; the window continues from the previous one.
    //   Soft — release notes still held, at the window start. A *musical* break
    //          (loop seam, Loop -> Song hand-off): the position moved but the
    //          transport did not, so controllers must keep their state.
    //   Hard — Soft plus Reset All Controllers + All Notes Off on every instrument
    //          channel. A *transport* break (stop, user seek, host relocate).
    enum class Reset { None, Soft, Hard };

    // Generate all MIDI for one musical bar window [prevBars, curBars). The caller
    // (renderCycle) must already hold snapMutex. Always returns true.
    bool renderWindowLocked(bool nowPlaying, Reset reset, double prevBars, double curBars);

    // Where in the musical window [prevBars, curBars) an armed switch should land:
    // the first position whose intra-bar phase matches pendingAtPhase, so the switch
    // is a whole number of bars and the beat never moves. False if this cycle does
    // not contain one (the switch simply waits — at most one bar). snapMutex must be
    // held.
    bool handoffPoint(double prevBars, double curBars, double& at) const;

    // Fill `out` with the current timeline + mode. Owner thread, no lock held: all
    // allocation happens here so the commit under snapMutex is a cheap move. Returns
    // false when there is nothing to publish, in which case the live snapshot must
    // be left alone rather than replaced with an empty one.
    bool buildSnapshot(Snapshot& out);

    // ── Owner-thread state ────────────────────────────────────────────────────
    LoopManager*                loopMgr       = nullptr;
    bool                             loopMode  = false;
    bool                        rebuildsSuspended = false;
    // Set while armScene() builds, so buildSnapshot() reads the scene being switched
    // to rather than the one still sounding. Owner thread only.
    bool                        buildingPendingScene = false;
    std::map<int, InstrumentRouting> instrumentMap_;

    double sampleRateHz = 48000.0;

    // Continuous reposition offset (see setSecsOffset).
    std::atomic<double> secsOffset{0.0};

    // An armed switch: pendingSnap is built and parked, and the RT thread adopts it
    // at the next frame whose intra-bar phase matches pendingAtPhase. All three are
    // armed under snapMutex together with pendingSnap, so the RT thread can never
    // adopt one half of a switch.
    //
    // There is one slot, and that is deliberate. Every arm comes from a user action
    // and a second one supersedes rather than queues — there is no musical meaning to
    // "scene 3 next bar, then scene 4 two bars later" — and more slots would mean
    // several full Snapshot copies held under the lock for nothing.
    //
    //   ToSong  the Loop -> Song hand-off: new tempo map, clock slid onto it
    //   ToLoop  the Song -> Loop hand-off: its mirror
    //   Scene   a Loop-mode scene change: content only. It must NOT touch the tempo
    //           map or secsOffset — the clock runs straight through a scene change,
    //           and sliding it would jump the loops.
    enum class Pending : uint8_t { None, ToSong, ToLoop, Scene };
    Pending pendingKind      = Pending::None;
    // The intra-bar phase the switch lands on. ToSong takes it from the resume bar
    // (an integer one gives 0, i.e. a bar line); the others are always bar lines.
    float   pendingAtPhase   = 0.0f;
    // ToSong only: the bar the song picks up at. Kept apart from the phase above
    // because a scene swap has no resume bar at all — overloading one field to mean
    // both "where to land" and "where to resume" is how this gets confusing later.
    float   pendingResumeBar = 0.0f;

    // Song-loop region + published wrapped position (see setSongLoop / renderCycle).
    std::atomic<bool>  songLoopOn{false};
    std::atomic<float> songLoopStart{0.0f};
    std::atomic<float> songLoopEnd{0.0f};
    std::atomic<float> loopedBar{0.0f};

    // RT-thread-only sub-segment time base, set by renderCycle() before each
    // renderWindowLocked() call and read by segFrameOffset() via emit().
    double segCycleStartSecs = 0.0;   // seconds from the cycle's first frame to segment start
    double segMusicalStart   = 0.0;   // musical bar at that instant
    double musicalPos        = 0.0;   // current wrapped musical position
    bool   loopCursorValid   = false; // musicalPos synced to the clock?

    // ── RT-thread-only state ──────────────────────────────────────────────────
    struct ActiveNote {
        int    midiPitch;
        int    channel;
        double offBar;           // bar position of the scheduled note-off
        char  portName[64];      // self-contained copy: a note can outlive its port
    };
    std::vector<ActiveNote> activeNotes;

    // Pending skipNoteOnce() entries. Fixed-size so the RT thread never allocates;
    // with more in flight than this the oldest is overwritten, which only brings the
    // echo back. Guarded by snapMutex.
    struct SkipNote {
        bool   live = false;
        char   portName[64] = {};   // empty: any port
        int    channel   = -1;      // 0-based; -1: any channel
        int    midiPitch = 0;
        double bar       = 0.0;
        double tol       = 0.0;
    };
    std::array<SkipNote, 16> skipNotes{};
    // Consume the entry matching this firing, if any. snapMutex must be held.
    bool consumeSkip(const std::string& port, int channel, int midiPitch, double onBar);

    // Reused scratch (pre-reserved in ctor; no per-cycle allocation).
    struct PendingParam {
        double             bar;
        const std::string* portName;
        int                midiChannel;
        int                ccNumber;
        int                value;
        int                priority;
    };
    std::vector<PendingParam>                        paramScratch;
    std::vector<std::pair<const std::string*, int>>  resetScratch;

    void fireNoteEvents (double prevBars, double curBars);
    void fireParamEvents(double prevBars, double curBars);
};

#endif
