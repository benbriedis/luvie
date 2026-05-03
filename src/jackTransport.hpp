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

    void setTimeline(ObservableSong* tl);
    void setActivePatterns(ActivePatternSet* aps);
    void setNoteParams(int rootPitch, int chordType);

    // Instrument routing: maps instrument name → (portName, 1-based MIDI channel).
    // Call from UI thread whenever the instruments list changes.
    struct InstrumentRouting {
        std::string instrumentName;
        std::string portName;
        int         midiChannel;   // 1-based
        int         programNumber = -1;  // -1 = not set; 0-127 = MIDI program
        int         bankMsb       = -1;  // -1 = not set; 0-127 = CC#0 value
        int         bankLsb       = -1;  // -1 = not set; 0-127 = CC#32 value
    };
    void setInstruments(const std::vector<InstrumentRouting>& routings);
    void sendProgramChange(const std::string& portName, int midiCh0,
                           int bankMsb, int bankLsb, int program);

    // MIDI port management — call from UI thread.
    bool addMidiPort(const std::string& name);
    bool removeMidiPort(const std::string& name);
    bool renameMidiPort(const std::string& oldName, const std::string& newName);

    std::function<void()> onTransportEvent;

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
    std::map<std::string, InstrumentRouting> instrumentMap_; // instrumentName → routing

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
        std::string    portName;   // empty = all ports
    };
    std::vector<ActiveNote> activeNotes;
    jack_nframes_t          lastFrame  = 0;
    bool                    wasPlaying = false;
    bool                    firstCall  = true;

    // ── JACK process callback ─────────────────────────────────────────────────
    static int processCallback(jack_nframes_t nframes, void* arg);
    int process(jack_nframes_t nframes);

    using NamedBuf = std::pair<std::string, void*>;
    void fireNoteEvents (const std::vector<NamedBuf>& namedBufs,
                         jack_nframes_t nframes,
                         jack_nframes_t blockStart, float prevBars, float curBars);
    void fireParamEvents(const std::vector<NamedBuf>& namedBufs,
                         jack_nframes_t nframes,
                         jack_nframes_t blockStart, float prevBars, float curBars);
};

#endif
