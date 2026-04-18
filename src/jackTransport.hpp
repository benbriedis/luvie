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
 *   - Outputs MIDI note events generated from the ObservableTimeline on a JACK MIDI port
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

    // Open JACK client. enableMidi=false skips MIDI port (transport control only).
    // Returns false on failure.
    bool open(const char* clientName = "luvie", bool enableMidi = true);

    // Called from UI thread when timeline or note params change.
    void setTimeline(ObservableTimeline* tl);
    void setNoteParams(int rootPitch, int chordType);

    // MIDI port management — call from UI thread.
    // addMidiPort registers a new JACK MIDI output port and returns true on success.
    bool addMidiPort(const std::string& name);
    bool removeMidiPort(const std::string& name);
    bool renameMidiPort(const std::string& oldName, const std::string& newName);

    // Called from the RT process thread when play/stop state changes or transport
    // jumps. Must be lightweight (e.g. just call Fl::awake). Set before open().
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

private:
    // ── JACK handles ──────────────────────────────────────────────────────────
    jack_client_t* client      = nullptr;
    jack_nframes_t sampleRate  = 48000;
    bool           midiEnabled = true;

    // Protected by portsMutex — modified on UI thread, read on RT thread.
    std::mutex                           portsMutex;
    std::map<std::string, jack_port_t*>  midiPorts_;

    // ── Atomics (shared between RT and UI threads) ────────────────────────────
    std::atomic<jack_nframes_t> posFrames{0};
    std::atomic<bool>           playing_{false};
    std::atomic<bool>           jackAlive{false};  // false after JACK server shutdown

    // ── UI-thread-only state ──────────────────────────────────────────────────
    ObservableTimeline* timeline  = nullptr;
    int                 rootPitch = 0;
    int                 chordType = 0;

    // ── Timeline snapshot for RT use ──────────────────────────────────────────
    struct TimeSegment {
        float  bar;
        float  bpm;
        int    beatsPerBar;
        double startSecs;   // seconds at the start of this segment (pre-computed)
    };
    struct NoteSnap {
        int   midiPitch;
        float beat;         // beat within pattern
        float length;       // beats
        float velocity;
    };
    struct InstanceSnap {
        float startBar;
        float length;       // instance length in bars
        float startOffset;  // beat offset into pattern at instance start
        float beatsPerBar;
        float patternBeats;
        std::vector<NoteSnap> notes;
    };
    struct TrackSnap {
        int channel;        // MIDI channel (0–15)
        std::vector<InstanceSnap> instances;
    };
    struct Snapshot {
        std::vector<TimeSegment> segs;
        std::vector<TrackSnap>   tracks;
    };
    mutable std::mutex snapMutex;
    Snapshot           snap;

    void   rebuildSnapshot();
    double snapBarToSeconds(float bar)    const;   // uses snap.segs — call with lock held
    float  snapSecondsToBar(double secs)  const;   // uses snap.segs — call with lock held

    // ── RT-thread-only state ──────────────────────────────────────────────────
    struct ActiveNote {
        int            midiPitch;
        int            channel;
        jack_nframes_t offFrame;   // absolute JACK frame when note-off fires
    };
    std::vector<ActiveNote> activeNotes;
    jack_nframes_t          lastFrame  = 0;
    bool                    wasPlaying = false;
    bool                    firstCall  = true;

    // ── JACK process callback (private) ──────────────────────────────────────
    static int processCallback(jack_nframes_t nframes, void* arg);
    int process(jack_nframes_t nframes);

    void fireNoteEvents(const std::vector<void*>& bufs, jack_nframes_t nframes,
                        jack_nframes_t blockStart, float prevBars, float curBars);
};

#endif
