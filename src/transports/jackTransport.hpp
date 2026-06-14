#ifndef JACK_TRANSPORT_HPP
#define JACK_TRANSPORT_HPP

#include "itransport.hpp"
#include "observableSong.hpp"
#include "activePatternSet.hpp"
#include "timeline.hpp"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/*
 * JackTransport — ITransport implementation that:
 *   - Drives play/pause/seek via the JACK transport API (so all JACK apps stay in sync)
 *   - Outputs MIDI note events generated from the ObservableSong on JACK MIDI ports
 *
 * Thread model:
 *   - All public ITransport methods and setters are called on the UI (main) thread.
 *   - processCallback() runs on the JACK real-time thread.
 *   - The Snapshot struct is protected by snapMutex. The RT thread uses try_lock;
 *     if it fails it skips note generation for that cycle (avoids priority inversion).
 */
class JackTransport : public ITransport, public ITimelineObserver, public IActivePatternObserver {
public:
    JackTransport();
    ~JackTransport();

    bool open(const char* clientName = "luvie", bool enableMidi = true);

    // Tear down a (possibly dead) JACK client so a fresh open() can reconnect.
    // Used by the reconnect path after the server has shut down; called on the
    // UI thread. Closes the client handle, drops the now-invalid MIDI ports and
    // resets RT-thread state. Safe to call on an already-closed transport.
    void close();

    // Suppress JACK's own console logging (call once before opening). Used to
    // keep the availability poll quiet unless the app is run in verbose mode.
    static void silenceLogging();

    void setTimeline(ObservableSong* tl);
    void setActivePatterns(ActivePatternSet* aps);
    void setNoteParams(int rootPitch, int chordType);

    // Instrument routing: maps instrument ID → (portName, 1-based MIDI channel).
    // Call from UI thread whenever the instruments list changes.
    struct InstrumentRouting {
        int         instrumentId;
        std::string portName;
        int         midiChannel;   // 1-based
        int         programNumber = -1;  // -1 = not set; 0-127 = MIDI program
        int         bankMsb       = -1;  // -1 = not set; 0-127 = CC#0 value
        int         bankLsb       = -1;  // -1 = not set; 0-127 = CC#32 value
    };
    void setInstruments(const std::vector<InstrumentRouting>& routings);
    void sendProgramChange(const std::string& portName, int midiCh0,
                           int bankMsb, int bankLsb, int program);

    // Queue a raw MIDI message (1-3 bytes) to be written to the named port on the
    // next RT cycle. Thread-safe; called from the UI thread (e.g. by JackPort).
    void enqueue(const std::string& portName, const uint8_t* data, int len);

    // MIDI port management — call from UI thread.
    bool addMidiPort(const std::string& name);
    bool removeMidiPort(const std::string& name);
    bool renameMidiPort(const std::string& oldName, const std::string& newName);

    std::function<void()> onTransportEvent;

    // Called when the JACK server shuts down or zombifies this client. Lets the
    // app drop back to the availability poll and reconnect when a server reappears.
    // Marshalled to the owner's thread via awakeFn (see below) when that is set.
    std::function<void()> onShutdown;

    // Marshals a callback to the owner's UI/main thread. The shutdown notification
    // fires from a JACK-internal thread; an owner with a UI event loop sets this
    // (e.g. to wrap Fl::awake) so onShutdown runs on its main thread instead. If
    // null (e.g. the headless LV2 DSP), the callback runs directly on the JACK thread.
    void (*awakeFn)(void (*)(void*), void*) = nullptr;

    // ITimelineObserver
    void onTimelineChanged()       override { rebuildSnapshot(); }

    // IActivePatternObserver
    void onActivePatternsChanged() override { rebuildSnapshot(); }

    // ITransport — called from UI thread
    void  play()           override;
    void  pause()          override;
    void  rewind()         override;
    void  seek(float bars) override;
    float position()  const override;
    bool  isPlaying() const override { return playing_.load(); }

    // True once a JACK client is open and live (so port registration / MIDI work).
    bool  isOpen() const { return client != nullptr && jackAlive.load(); }
    void  setLoopMode(bool loopMode) override;

private:
    // ── JACK handles ──────────────────────────────────────────────────────────
    jack_client_t* client      = nullptr;
    jack_nframes_t sampleRate  = 48000;
    bool           midiEnabled = true;

    std::mutex                           portsMutex;
    std::map<std::string, jack_port_t*>  midiPorts_;

    struct PendingMsg { std::string portName; uint8_t data[3]; int len; };
    std::mutex              pendingMutex_;
    std::vector<PendingMsg> pendingMsgs_;

    // ── Atomics ───────────────────────────────────────────────────────────────
    std::atomic<jack_nframes_t> posFrames{0};
    std::atomic<bool>           playing_{false};
    std::atomic<bool>           jackAlive{false};

    // ── UI-thread-only state ──────────────────────────────────────────────────
    ObservableSong*              timeline     = nullptr;
    ActivePatternSet*                aps          = nullptr;
    int                              rootPitch    = 0;
    int                              chordType    = 0;
    bool                             loopMode     = false;
    std::map<int, InstrumentRouting> instrumentMap_; // instrumentId → routing

    // ── Timeline snapshot for RT use ─────────────────────────────────────────
    struct TimeSegment {
        float  bar;
        float  bpm;
        int    beatsPerBar;
        double startSecs;
    };
    struct NoteSnap {
        int   midiPitch;
        float beat;
        float length;
        float velocity;
    };
    struct InstanceSnap {
        float startBar;
        float length;
        float startOffset;
        float beatsPerBar;
        float patternBeats;
        bool  loop = false;      // loop-mode instance: anchor is phase only, plays forever
        std::string portName;    // JACK port name to use; empty = all ports
        int   midiChannel = 0;  // 0-based MIDI channel
        std::vector<NoteSnap> notes;
    };
    struct TrackSnap {
        std::vector<InstanceSnap> instances;
    };
    struct ParamEventSnap {
        float beat;   // within-pattern beat (pattern-level) or bar position (song-level)
        int   value;  // 0-127 for CC, 0-16383 for pitch bend
    };
    struct ParamInstSnap {
        float startBar;
        float length;        // bars
        float startOffset;   // beats offset into pattern at instance start; 0 for song-level
        float beatsPerBar;   // 1.0f for song-level (beat == bar)
        float patternBeats;  // 0.0f = song-level (no loop); >0 = pattern length in beats
        bool  loop = false;  // loop-mode instance: anchor is phase only, plays forever
        std::string portName;
        int   midiChannel;   // 0-based
        int   priority;      // 0 = song-level; trackIdx+1 for pattern lanes
        int   ccNumber;      // 1,7,10,11; -1 = pitch bend
        std::vector<ParamEventSnap> events;  // sorted by beat
    };
    struct Snapshot {
        std::vector<TimeSegment>   segs;
        std::vector<TrackSnap>     tracks;
        std::vector<ParamInstSnap> paramInsts;
    };
    mutable std::mutex snapMutex;
    Snapshot           snap;

    void   rebuildSnapshot();
    double snapBarToSeconds(float bar)    const;
    float  snapSecondsToBar(double secs)  const;

    // ── RT-thread-only state ──────────────────────────────────────────────────
    struct ActiveNote {
        int            midiPitch;
        int            channel;
        jack_nframes_t offFrame;
        // Self-contained copy of the port name (not a pointer): an active note
        // can outlive its port if the port is removed mid-playback, so it must
        // not reference the now-freed midiPorts_ key. POD copy => no per-note
        // heap allocation when pushed onto activeNotes. Empty = all ports.
        char           portName[64];
    };
    std::vector<ActiveNote> activeNotes;
    jack_nframes_t          lastFrame  = 0;
    bool                    wasPlaying = false;
    bool                    firstCall  = true;

    // All MIDI output for a cycle is collected here, then sorted by frame per
    // buffer and written at the end of process(). JACK drops any event whose
    // frame is earlier than the last one already written to a buffer, so the
    // note-off / note-on / param passes must be merged into one ordered stream.
    // Reused across cycles to avoid per-cycle allocation on the RT thread.
    struct OutEvent {
        void*          buf;
        jack_nframes_t frame;
        uint32_t       seq;     // insertion order; tiebreak so sort stays in-place
        uint8_t        data[3];
        int            len;
    };
    std::vector<OutEvent> outEvents;

    // Per-cycle scratch reused across cycles so the RT callback allocates nothing
    // — all are pre-reserved in the constructor so even the first process() call
    // is allocation-free. NamedBuf holds a pointer to the stable midiPorts_ map
    // key (valid for the cycle), not a string copy.
    using NamedBuf = std::pair<const std::string*, void*>;
    std::vector<NamedBuf>  namedBufs;

    // One pending CC/pitch-bend event awaiting priority resolution in fireParamEvents.
    struct PendingParam {
        jack_nframes_t     frame;
        const std::string* portName;
        int                midiChannel;
        int                ccNumber;
        int                value;
        int                priority;
    };
    std::vector<PendingParam> paramScratch;

    // Dedup scratch for the stop/jump controller-reset pass: (port name, channel).
    std::vector<std::pair<const std::string*, int>> resetScratch;

    // ── JACK process callback ─────────────────────────────────────────────────
    static int processCallback(jack_nframes_t nframes, void* arg);
    int process(jack_nframes_t nframes);

    // JACK server-shutdown callback (runs on a JACK-internal thread).
    static void shutdownThunk(void* arg);

    void fireNoteEvents (const std::vector<NamedBuf>& namedBufs,
                         jack_nframes_t nframes,
                         jack_nframes_t blockStart, float prevBars, float curBars);
    void fireParamEvents(const std::vector<NamedBuf>& namedBufs,
                         jack_nframes_t nframes,
                         jack_nframes_t blockStart, float prevBars, float curBars);

    // Queue one event for the current cycle (collected, sorted, then written).
    void emit(void* buf, jack_nframes_t frame, const uint8_t* data, int len);
};

#endif
