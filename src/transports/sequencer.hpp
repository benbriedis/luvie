#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include "itimelineobserver.hpp"
#include "observableSong.hpp"
#include "loopManager.hpp"
#include "timeline.hpp"
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
 *   - renderWindow() runs on the real-time thread. It try_locks snapMutex; on
 *     failure it does nothing and returns false so the caller leaves wasPlaying
 *     untouched (the missed stop/jump is handled next cycle).
 *   - emit() is called only from renderWindow() (RT thread) and must not allocate.
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

    // ITimelineObserver / ILoopObserver
    void onTimelineChanged()       override { rebuildSnapshot(); }
    void onLoopsChanged() override { rebuildSnapshot(); }

protected:
    // ── Snapshot (RT-readable copy of the timeline) ───────────────────────────
    // cpm = crotchets per minute (the BPM markers scaled by the beat definition);
    // barCrotchets comes from the time signature. One bar lasts
    // barCrotchets * 60 / cpm. beatsPerBar is the numerator — the grid's beat,
    // which is what note positions are measured in and is a different unit.
    struct TimeSegment { float bar; float cpm; int beatsPerBar; double barCrotchets; double startSecs; };
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

    void   rebuildSnapshot();
    double snapBarToSeconds(float bar)   const;   // RT-safe; reads snap.segs
    float  snapSecondsToBar(double secs) const;

    // ── RT firing ─────────────────────────────────────────────────────────────
    // Generate all MIDI for one cycle's bar window. nowPlaying/jumped are computed
    // by the backend from its clock. Returns true if it ran (snapMutex acquired);
    // false means the caller should leave wasPlaying unchanged. Calls emit() for
    // every message produced. wasPlaying is read here but updated by the caller.
    bool renderWindow(bool nowPlaying, bool jumped, float prevBars, float curBars);

    bool wasPlaying = false;   // owner: updated by the backend after renderWindow

    // Backend output hook: emit one 1-3 byte MIDI message for `port` at bar
    // position `bar` within the current cycle. Called on the RT thread; must not
    // allocate. The backend converts `bar` to its frame/time domain.
    virtual void emit(const std::string& port, float bar,
                      const uint8_t* data, int len) = 0;

private:
    // ── Owner-thread state ────────────────────────────────────────────────────
    LoopManager*                loopMgr       = nullptr;
    bool                             loopMode  = false;
    std::map<int, InstrumentRouting> instrumentMap_;

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
