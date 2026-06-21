#ifndef JACK_TRANSPORT_HPP
#define JACK_TRANSPORT_HPP

#include "itransport.hpp"
#include "sequencer.hpp"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/*
 * JackTransport — an ITransport that drives the shared Sequencer from the JACK
 * transport and writes its MIDI to JACK MIDI output ports (used by the standalone
 * app / NSM client). It:
 *   - Drives play/pause/seek via the JACK transport API (so all JACK apps stay in sync)
 *   - Converts each Sequencer emit() (port + bar) into a JACK MIDI event on the
 *     matching port, collected and written in frame order per cycle.
 *
 * Thread model:
 *   - Public ITransport methods and the inherited Sequencer setters run on the UI thread.
 *   - processCallback() runs on the JACK real-time thread and calls renderWindow().
 */
class JackTransport : public Sequencer, public ITransport {
public:
    JackTransport();
    ~JackTransport() override;

    bool open(const char* clientName = "luvie", bool enableMidi = true);

    // Tear down a (possibly dead) JACK client so a fresh open() can reconnect.
    // Closes the client handle, drops the now-invalid MIDI ports and resets
    // RT-thread state. Safe to call on an already-closed transport.
    void close();

    // Suppress JACK's own console logging (call once before opening).
    static void silenceLogging();

    void sendProgramChange(const std::string& portName, int midiCh0,
                           int bankMsb, int bankLsb, int program);

    // Queue a raw MIDI message (1-3 bytes) to the named port on the next RT cycle.
    // Thread-safe; called from the UI thread (e.g. by JackPort).
    void enqueue(const std::string& portName, const uint8_t* data, int len);

    // MIDI port management — call from UI thread.
    bool addMidiPort(const std::string& name);
    bool removeMidiPort(const std::string& name);
    bool renameMidiPort(const std::string& oldName, const std::string& newName);

    std::function<void()> onTransportEvent;

    // Called when the JACK server shuts down or zombifies this client.
    std::function<void()> onShutdown;

    // Marshals a callback to the owner's UI/main thread (e.g. wraps Fl::awake). If
    // null (e.g. the headless LV2 DSP), the callback runs on the JACK thread.
    void (*awakeFn)(void (*)(void*), void*) = nullptr;

    // ITransport — called from UI thread
    void  play()           override;
    void  pause()          override;
    void  rewind()         override;
    void  seek(float bars) override;
    float position()  const override;
    bool  isPlaying() const override { return playing_.load(); }
    void  setLoopMode(bool loopMode) override { Sequencer::setLoopMode(loopMode); }

    // True once a JACK client is open and live (so port registration / MIDI work).
    bool  isOpen() const { return client != nullptr && jackAlive.load(); }

protected:
    // Sequencer output hook (RT thread): resolve the port's cycle buffer and queue
    // the message at the bar's frame offset for the ordered end-of-cycle flush.
    void emit(const std::string& port, float bar,
              const uint8_t* data, int len) override;

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

    // ── RT-thread-only state ──────────────────────────────────────────────────
    jack_nframes_t lastFrame  = 0;
    bool           firstCall  = true;

    // Per-cycle context for emit(): the buffers, this cycle's start frame and length.
    using NamedBuf = std::pair<const std::string*, void*>;
    std::vector<NamedBuf> namedBufs;
    jack_nframes_t        curBlockStart = 0;
    jack_nframes_t        curNframes    = 0;

    // All MIDI output for a cycle is collected here, then sorted by frame per
    // buffer and written at the end of process(). JACK drops any event whose frame
    // is earlier than the last one already written to a buffer. Reused across
    // cycles to avoid per-cycle allocation on the RT thread.
    struct OutEvent {
        void*          buf;
        jack_nframes_t frame;
        uint32_t       seq;      // insertion order; tiebreak so sort stays in-place
        uint8_t        data[3];
        int            len;
    };
    std::vector<OutEvent> outEvents;

    // ── JACK process callback ─────────────────────────────────────────────────
    static int processCallback(jack_nframes_t nframes, void* arg);
    int process(jack_nframes_t nframes);

    static void shutdownThunk(void* arg);
};

#endif
