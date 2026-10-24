// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef JACK_TRANSPORT_HPP
#define JACK_TRANSPORT_HPP

#include "itransport.hpp"
#include "cycleDriver.hpp"
#include "midiBackend.hpp"   // kMaxMidiInputs
#include <jack/jack.h>
#include <jack/midiport.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/*
 * JackTransport — an ITransport that drives the shared CycleDriver from the JACK
 * transport and writes its MIDI to JACK MIDI output ports (used by the standalone
 * app / NSM client). It:
 *   - Drives play/pause/seek via the JACK transport API (so all JACK apps stay in sync)
 *   - Resolves each emitted message's port to this cycle's JACK buffer; the driver
 *     collects them, and process() writes them out in frame order.
 *
 * Thread model:
 *   - Public ITransport methods and the inherited Sequencer setters run on the UI thread.
 *   - processCallback() runs on the JACK real-time thread and calls renderCycle().
 */
class MidiInputManager;
class MidiScheduler;

class JackTransport : public CycleDriver, public ITransport {
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

    // MIDI input ports, one per MidiInputManager slot that is backed by Jack.
    // Events read in process() are handed to the sink on the RT thread, tagged
    // with the slot, which queues them lock-free for the UI thread — see
    // MidiInputManager. Null sink means no input is read at all.
    bool addMidiInPort(int slot, const std::string& name);
    bool removeMidiInPort(int slot);
    bool renameMidiInPort(int slot, const std::string& newName);
    void setMidiInSink(MidiInputManager* s);

    std::function<void()> onTransportEvent;

    // Called when the JACK server shuts down or zombifies this client.
    std::function<void()> onShutdown;

    // Marshals a callback to the owner's UI/main thread (e.g. wraps Fl::awake). If
    // null (e.g. the headless LV2 DSP), the callback runs on the JACK thread.
    void (*awakeFn)(void (*)(void*), void*) = nullptr;

    // ITransport — called from UI thread
    void  play()             override;
    void  pause()            override;
    void  rewind()           override;
    void  seek(float bars)   override;
    void  reanchor(float bars) override;
    float position()  const override;
    bool  isPlaying() const override { return playing_.load(); }
    void  setLoopMode(bool loopMode) override { Sequencer::setLoopMode(loopMode); }
    void  setSongLoop(bool enabled, float startBar, float endBar) override {
        Sequencer::setSongLoop(enabled, startBar, endBar);
    }
    // No jack_transport_locate: the Sequencer shifts its bar offset and swaps to the
    // song snapshot on the same RT cycle, leaving the JACK timeline rolling.
    void  endLoopMode(float bars)    override { Sequencer::endLoopMode(bars); }
    void  armScene(float atBar)      override { Sequencer::armScene(atBar); }
    void  beginLoopMode(float atBar) override { Sequencer::beginLoopMode(atBar); }
    void  skipNoteOnce(int instrumentId, int midiPitch, double bar, double tolBars) override {
        Sequencer::skipNoteOnce(instrumentId, midiPitch, bar, tolBars);
    }

    // True once a JACK client is open and live (so port registration / MIDI work).
    bool  isOpen() const { return client != nullptr && jackAlive.load(); }

    // Messages for ports this client has no JACK buffer for (Native and Debug ports)
    // go to `sched`, timestamped, rather than being dropped. This engine only renders
    // while the JACK transport rolls, and the TransportRouter stops it whenever JACK
    // stops being the clock, so in practice that is "while JACK is the clock" — and the
    // stop it renders then still reaches those ports, releasing their notes. Set once,
    // before the client opens. Standalone only.
    void setScheduler(MidiScheduler* sched) { scheduler_ = sched; }

protected:
    // CycleDriver hooks (RT thread): a port with a JACK buffer this cycle is
    // buffered and flushed in frame order; any other goes to the scheduler.
    int  resolvePort(const std::string& port) override;
    void emitUnbuffered(const std::string& port, long offset,
                        const uint8_t* data, int len) override;
    // The server shut the client down: no process callback will drain the queue.
    bool rtAlive() const override { return jackAlive.load(); }

private:
    // ── JACK handles ──────────────────────────────────────────────────────────
    jack_client_t* client      = nullptr;
    bool           midiEnabled = true;

    std::mutex                           portsMutex;
    std::map<std::string, jack_port_t*>  midiPorts_;
    jack_port_t*                         midiInPorts_[kMaxMidiInputs] = {};   // by slot; guarded by portsMutex
    std::atomic<MidiInputManager*>       midiInSink_{nullptr};

    // Trivially destructible, so the RT thread's clear() frees nothing.
    struct PendingMsg { char portName[64]; uint8_t data[3]; int len; };
    std::mutex              pendingMutex_;
    std::vector<PendingMsg> pendingMsgs_;

    // ── Atomics ───────────────────────────────────────────────────────────────
    std::atomic<jack_nframes_t> posFrames{0};
    std::atomic<bool>           playing_{false};
    std::atomic<bool>           jackAlive{false};
    // The timeline shift used by reanchor() and by the Loop -> Song hand-off lives
    // in the Sequencer base (setSecsOffset / secsOffsetSecs) so the plugin gets the
    // same mechanism; seek()/rewind() clear it, since an explicit reposition
    // re-establishes the identity frame<->bar mapping.

    // ── RT-thread-only state ──────────────────────────────────────────────────
    jack_nframes_t lastFrame   = 0;
    bool           lastRolling = false;   // the transport's state last cycle
    bool           firstCall   = true;

    // Per-cycle context for the CycleDriver hooks: the port buffers, indexed by
    // what resolvePort() returns, and when the cycle started.
    using NamedBuf = std::pair<const std::string*, void*>;
    std::vector<NamedBuf> namedBufs;
    int64_t               cycleStartNs  = 0;   // rtNowNs() at the top of process()

    MidiScheduler* scheduler_ = nullptr;

    // ── JACK process callback ─────────────────────────────────────────────────
    static int processCallback(jack_nframes_t nframes, void* arg);
    int process(jack_nframes_t nframes);

    static void shutdownThunk(void* arg);
};

#endif
