// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include "itimelineobserver.hpp"
#include "observableSong.hpp"
#include "loopManager.hpp"
#include "timeline.hpp"
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
class Sequencer : public ITimelineObserver, public ILoopObserver {
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
    // held are released, the pre-built snapshot is swapped in and the bar offset is
    // shifted by a whole number of bars so the position becomes exactly resumeBar. The
    // backend clock is never relocated and controllers are never reset. Owner thread.
    void endLoopMode(float resumeBar);

    // Drop a hand-off that is armed but has not landed yet — the user clicked back
    // into Loop mode, or seeked. setLoopMode() calls it, so entering a mode always
    // supersedes a pending exit. Owner thread; briefly blocks on snapMutex.
    void cancelHandoff();

    // Suspend snapshot rebuilds while a multi-step change is applied (the mode and
    // the active loop set together), so the RT thread sees one commit rather than an
    // intermediate snapshot at the old position. The caller must finish with
    // rebuildSnapshot() or endLoopMode(), which commit regardless. Owner thread.
    void suspendRebuilds(bool on) { rebuildsSuspended = on; }

    // ITimelineObserver / ILoopObserver
    void onTimelineChanged()       override { rebuildSnapshot(); }
    void onLoopsChanged() override { rebuildSnapshot(); }

protected:
    // ── Snapshot (RT-readable copy of the timeline) ───────────────────────────
    // The tempo table is a straight copy of ObservableSong::tempoMap() — see
    // timeSettings::TempoSegment for what it holds and why a linear tempo ramp
    // needs no special case here.
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

    // Musical bar added to every frame->bar conversion (and subtracted in bar->frame),
    // so the playhead can be repositioned without relocating the backend clock. Used
    // by the tempo re-anchor and by the Loop -> Song hand-off; renderCycle() clears
    // it on any clock jump (a seek of ours, or a host relocate), which re-establishes
    // the identity mapping. Owner thread writes, RT thread reads; lock-free.
    void   setBarOffset(double off) { barOffset.store(off, std::memory_order_relaxed); }
    double barOffsetBars() const    { return barOffset.load(std::memory_order_relaxed); }
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

    // Where in the musical window [prevBars, curBars) an armed hand-off should land:
    // the first position whose intra-bar phase matches pendingResumeBar's, so the
    // switch is a whole number of bars and the beat never moves. False if this cycle
    // does not contain one (the hand-off simply waits — at most one bar). snapMutex
    // must be held.
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
    std::map<int, InstrumentRouting> instrumentMap_;

    double sampleRateHz = 48000.0;

    // Continuous reposition offset (see setBarOffset).
    std::atomic<double> barOffset{0.0};

    // Loop -> Song hand-off, armed by endLoopMode() under snapMutex together with
    // pendingSnap and applied by the RT thread at the next matching bar phase.
    // Guarded by snapMutex, so the RT thread can never adopt one half of the switch.
    bool  pendingHandoff   = false;
    float pendingResumeBar = 0.0f;

    // Song-loop region + published wrapped position (see setSongLoop / renderCycle).
    std::atomic<bool>  songLoopOn{false};
    std::atomic<float> songLoopStart{0.0f};
    std::atomic<float> songLoopEnd{0.0f};
    std::atomic<float> loopedBar{0.0f};

    // RT-thread-only sub-segment time base, set by renderCycle() before each
    // renderWindowLocked() call and read by segFrameOffset() via emit().
    double segCycleStartSecs = 0.0;   // seconds from the cycle's first frame to segment start
    double segMusicalStart   = 0.0;   // musical bar at that instant
    double emitBarOffset     = 0.0;   // backend frame->bar shift for this cycle
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
