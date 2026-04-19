#ifndef JACK_TRANSPORT_HPP
#define JACK_TRANSPORT_HPP

#include "itransport.hpp"
#include "observableTimeline.hpp"
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
 *   - Outputs MIDI note events generated from the ObservableTimeline on JACK MIDI ports
 *
 * Thread model:
 *   - All public ITransport methods and setters are called on the UI (main) thread.
 *   - processCallback() runs on the JACK real-time thread.
 *   - The Snapshot struct is protected by snapMutex. The RT thread uses try_lock;
 *     if it fails it skips note generation for that cycle (avoids priority inversion).
 */
class JackTransport : public ITransport, public ITimelineObserver {
public:
    JackTransport();
    ~JackTransport();

    bool open(const char* clientName = "luvie", bool enableMidi = true);

    void setTimeline(ObservableTimeline* tl);
    void setNoteParams(int rootPitch, int chordType);

    // Channel routing: maps channel name → (portName, 1-based MIDI channel).
    // Call from UI thread whenever the channels list changes.
    struct ChannelRouting {
        std::string channelName;
        std::string portName;
        int         midiChannel;  // 1-based
    };
    void setChannels(const std::vector<ChannelRouting>& routings);

    // MIDI port management — call from UI thread.
    bool addMidiPort(const std::string& name);
    bool removeMidiPort(const std::string& name);
    bool renameMidiPort(const std::string& oldName, const std::string& newName);

    std::function<void()> onTransportEvent;

    // ITimelineObserver
    void onTimelineChanged() override { rebuildSnapshot(); }

    // ITransport — called from UI thread
    void  play()           override;
    void  pause()          override;
    void  rewind()         override;
    void  seek(float bars) override;
    float position()  const override;
    bool  isPlaying() const override { return playing_.load(); }
    void  setLoopMode(bool loopMode, std::function<bool(int)> enabledFn) override;

private:
    // ── JACK handles ──────────────────────────────────────────────────────────
    jack_client_t* client      = nullptr;
    jack_nframes_t sampleRate  = 48000;
    bool           midiEnabled = true;

    std::mutex                           portsMutex;
    std::map<std::string, jack_port_t*>  midiPorts_;

    // ── Atomics ───────────────────────────────────────────────────────────────
    std::atomic<jack_nframes_t> posFrames{0};
    std::atomic<bool>           playing_{false};
    std::atomic<bool>           jackAlive{false};

    // ── UI-thread-only state ──────────────────────────────────────────────────
    ObservableTimeline*              timeline     = nullptr;
    int                              rootPitch    = 0;
    int                              chordType    = 0;
    bool                             loopMode     = false;
    std::map<std::string, ChannelRouting> channelMap_; // channelName → routing

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
    struct Snapshot {
        std::vector<TimeSegment> segs;
        std::vector<TrackSnap>   tracks;
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
    void fireNoteEvents(const std::vector<NamedBuf>& namedBufs,
                        jack_nframes_t nframes,
                        jack_nframes_t blockStart, float prevBars, float curBars);
};

#endif
