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
 *     also owns the song-loop wrap, splitting the cycle at the loop seam.
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
    };

    mutable std::mutex snapMutex;
    Snapshot           snap;

    // The data model. Set via setTimeline(); read by backends (e.g. for UI-thread
    // position/seek conversions). Owned by the caller.
    ObservableSong* timeline = nullptr;

    void   setSampleRate(double sr) { sampleRateHz = sr; }
    bool   songLoopActive() const { return songLoopOn.load(std::memory_order_relaxed); }
    // The (possibly wrapped) musical bar playback is currently at — for the UI
    // playhead. Published by renderCycle() every cycle; lock-free.
    float  loopedPosition() const { return loopedBar.load(std::memory_order_relaxed); }

    void   rebuildSnapshot();
    double snapBarToSeconds(float bar)   const;   // reads snap.segs; hold snapMutex
    float  snapSecondsToBar(double secs) const;

    // ── RT firing ─────────────────────────────────────────────────────────────
    // Render one RT cycle. cycleStartSecs is the backend clock's position on
    // Luvie's timeline (frame / sample-rate); barOffset is any backend frame->bar
    // shift (JACK's tempo re-anchor; 0 elsewhere). Acquires snapMutex once for the
    // whole cycle, converts the clock window to a musical window, and — when the
    // song loop is armed and playing — splits it at the loop seam so playback
    // wraps seamlessly. Returns true if the snapshot was readable and at least one
    // sub-window rendered; false means the caller should leave wasPlaying
    // unchanged (missed stop/jump retried next cycle). Calls emit() for every
    // message produced.
    bool renderCycle(bool nowPlaying, bool jumped, uint32_t nframes,
                     double cycleStartSecs, double barOffset);

    // Frame offset within the current cycle for an emitted musical `bar`, honouring
    // the active sub-segment's time base (so events past a loop wrap land at the
    // right frame). Call from emit(); result is not clamped to [0, nframes).
    long segFrameOffset(float bar) const;

    bool wasPlaying = false;   // owner: updated by the backend after renderCycle

    // Backend output hook: emit one 1-3 byte MIDI message for `port` at bar
    // position `bar` within the current cycle. Called on the RT thread; must not
    // allocate. The backend converts `bar` to its frame/time domain.
    virtual void emit(const std::string& port, float bar,
                      const uint8_t* data, int len) = 0;

private:
    // Generate all MIDI for one musical bar window [prevBars, curBars). The caller
    // (renderCycle) must already hold snapMutex. nowPlaying/jumped drive the
    // stop/jump reset; on a wrap the next sub-window is passed jumped=true so notes
    // held across the seam are released. Always returns true.
    bool renderWindowLocked(bool nowPlaying, bool jumped, float prevBars, float curBars);

    // ── Owner-thread state ────────────────────────────────────────────────────
    LoopManager*                loopMgr       = nullptr;
    bool                             loopMode  = false;
    std::map<int, InstrumentRouting> instrumentMap_;

    double sampleRateHz = 48000.0;

    // Song-loop region + published wrapped position (see setSongLoop / renderCycle).
    std::atomic<bool>  songLoopOn{false};
    std::atomic<float> songLoopStart{0.0f};
    std::atomic<float> songLoopEnd{0.0f};
    std::atomic<float> loopedBar{0.0f};

    // RT-thread-only sub-segment time base, set by renderCycle() before each
    // renderWindowLocked() call and read by segFrameOffset() via emit().
    double segCycleStartSecs = 0.0;   // seconds from the cycle's first frame to segment start
    float  segMusicalStart   = 0.0f;  // musical bar at that instant
    double emitBarOffset     = 0.0;   // backend frame->bar shift for this cycle
    float  musicalPos        = 0.0f;  // current wrapped musical position
    bool   loopCursorValid   = false; // musicalPos synced to the clock?

    // ── RT-thread-only state ──────────────────────────────────────────────────
    struct ActiveNote {
        int   midiPitch;
        int   channel;
        float offBar;            // bar position of the scheduled note-off
        char  portName[64];      // self-contained copy: a note can outlive its port
    };
    std::vector<ActiveNote> activeNotes;

    // Reused scratch (pre-reserved in ctor; no per-cycle allocation).
    struct PendingParam {
        float              bar;
        const std::string* portName;
        int                midiChannel;
        int                ccNumber;
        int                value;
        int                priority;
    };
    std::vector<PendingParam>                        paramScratch;
    std::vector<std::pair<const std::string*, int>>  resetScratch;

    void fireNoteEvents (float prevBars, float curBars);
    void fireParamEvents(float prevBars, float curBars);
};

#endif
