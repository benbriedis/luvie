// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "jackTransport.hpp"
#include "midiInPort.hpp"
#include "midiScheduler.hpp"
#include "rtThread.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// ── Construction / destruction ────────────────────────────────────────────────

JackTransport::JackTransport()
    // The event buffer is sized for the worst dense window a JACK cycle can produce
    // (param-ramp densification is capped per segment) and never grows on the RT
    // thread. The Sequencer base pre-reserves its own scratch.
    : CycleDriver(48000.0, 4096)
{
    namedBufs.reserve(64);
    pendingMsgs_.reserve(64);
}

JackTransport::~JackTransport()
{
    if (client && jackAlive.load()) {
        jack_deactivate(client);
        for (auto& [name, port] : midiPorts_)
            jack_port_unregister(client, port);
        jack_client_close(client);
    }
    attachRt(false);
}

// ── Open ──────────────────────────────────────────────────────────────────────

static void jackSilentLog(const char*) {}

void JackTransport::silenceLogging()
{
    // Route JACK's own console logging to a no-op so the once-per-second
    // availability poll doesn't spam the terminal with "cannot connect to
    // server" messages. Call once before opening; skipped in verbose mode.
    jack_set_error_function(jackSilentLog);
    jack_set_info_function(jackSilentLog);
}

bool JackTransport::open(const char* clientName, bool enableMidi)
{
    midiEnabled = enableMidi;

    // JackNoStartServer: connect only to an already-running server. Luvie waits
    // (polls) for an external JACK server rather than spawning one itself.
    client = jack_client_open(clientName, JackNoStartServer, nullptr);
    if (!client)
        return false;

    setDriverSampleRate(static_cast<double>(jack_get_sample_rate(client)));

    jack_set_process_callback(client, processCallback, this);
    // From the first process callback on, the RT thread owns the snapshot and the
    // setters reach it as commands.
    attachRt(true);

    // Detect the server going away without polling: JACK invokes this from one
    // of its own threads, so the thunk only flips an atomic and hands off to the
    // main thread via Fl::awake.
    jack_on_shutdown(client, &JackTransport::shutdownThunk, this);

    if (jack_activate(client) != 0) {
        fprintf(stderr, "JackTransport: could not activate JACK client\n");
        jack_client_close(client);
        client = nullptr;
        attachRt(false);
        return false;
    }

    jackAlive.store(true);
    return true;
}

// ── Shutdown handling ─────────────────────────────────────────────────────────

void JackTransport::shutdownThunk(void* arg)
{
    // Runs on a JACK-internal thread. The client is already dead here, so we
    // must not call any JACK functions — just mark the transport down and run the
    // onShutdown handler, marshalled to the owner's thread via awakeFn if set.
    auto* self = static_cast<JackTransport*>(arg);
    self->jackAlive.store(false);
    self->playing_.store(false);
    auto deliver = [](void* a) {
        auto* s = static_cast<JackTransport*>(a);
        if (s->onShutdown) s->onShutdown();
    };
    if (self->awakeFn) self->awakeFn(deliver, self);
    else               deliver(self);
}

void JackTransport::close()
{
    // Reached after the server has shut down (client is a zombie) or while
    // tearing down for reconnect. Free the client handle — JACK requires
    // jack_client_close() even on a dead client — and drop the MIDI ports,
    // whose handles belonged to the old client and are now invalid.
    if (client) {
        jack_client_close(client);
        client = nullptr;
    }
    // No process callback can run now: setters apply directly again.
    attachRt(false);
    {
        std::lock_guard<std::mutex> lk(portsMutex);
        midiPorts_.clear();
        for (auto& p : midiInPorts_) p = nullptr;
    }
    jackAlive.store(false);

    // Reset RT-thread state so a reconnected client starts clean. activeNotes is
    // owned by the Sequencer base; a fresh transport drops any sustained notes
    // along with the client, so leave them — the next stop/jump path resets cleanly.
    lastFrame   = 0;
    lastRolling = false;
    wasPlaying  = false;
    firstCall   = true;
}

// ── Pending raw-MIDI queue (UI thread) ─────────────────────────────────────────

void JackTransport::enqueue(const std::string& portName, const uint8_t* data, int len)
{
    if (!client || !midiEnabled || len < 1 || len > 3) return;
    PendingMsg m{{}, {0, 0, 0}, len};
    std::snprintf(m.portName, sizeof(m.portName), "%s", portName.c_str());
    for (int i = 0; i < len; i++) m.data[i] = data[i];
    std::lock_guard<std::mutex> lk(pendingMutex_);
    pendingMsgs_.push_back(std::move(m));
}

void JackTransport::sendProgramChange(const std::string& portName, int midiCh0,
                                      int bankMsb, int bankLsb, int program)
{
    uint8_t ch = static_cast<uint8_t>(midiCh0 & 0x0F);
    if (bankMsb >= 0) {
        uint8_t m[3] = {static_cast<uint8_t>(0xB0 | ch), 0,  static_cast<uint8_t>(bankMsb)};
        enqueue(portName, m, 3);
    }
    if (bankLsb >= 0) {
        uint8_t m[3] = {static_cast<uint8_t>(0xB0 | ch), 32, static_cast<uint8_t>(bankLsb)};
        enqueue(portName, m, 3);
    }
    if (program >= 0) {
        uint8_t m[2] = {static_cast<uint8_t>(0xC0 | ch), static_cast<uint8_t>(program)};
        enqueue(portName, m, 2);
    }
}

// ── ITransport methods (UI thread) ───────────────────────────────────────────

void JackTransport::play()  { if (client && jackAlive.load()) jack_transport_start(client); }
void JackTransport::pause() { if (client && jackAlive.load()) jack_transport_stop(client); }
void JackTransport::rewind(){
    if (!client || !jackAlive.load()) return;
    setSecsOffset(0.0);
    cancelPending();
    jack_transport_locate(client, 0);
}

void JackTransport::seek(float bars)
{
    if (!client || !jackAlive.load() || !timeline) return;
    // An explicit seek re-establishes the identity frame<->bar mapping: drop any
    // accumulated tempo re-anchor offset and locate straight to the target bar.
    setSecsOffset(0.0);
    cancelPending();
    double secs  = timeline->barToSeconds(std::max(0.0f, bars));
    auto   frame = static_cast<jack_nframes_t>(secs * sampleRate());
    jack_transport_locate(client, frame);
}

void JackTransport::reanchor(float bars)
{
    if (!client || !jackAlive.load() || !timeline) return;
    // A tempo-map change (anchored at bar 0) remapped every frame to a new bar,
    // so the playhead would scrub. Instead of relocating JACK — which dips
    // through JackTransportStarting, flickering play/pause and silencing notes —
    // leave the frames rolling and slide the song's timeline in time so the current
    // frame still reads as `bars` (the pre-change position sampled by the caller).
    // Future frames then advance at the new tempo. No reposition, so the RT thread
    // sees a contiguous window and keeps its notes sounding.
    double secs = static_cast<double>(posFrames.load()) / sampleRate();
    setSecsOffset(secs - timeline->barToSeconds(bars));
}

float JackTransport::position() const
{
    if (!timeline) return 0.0f;
    // While the song loop is armed the RT thread wraps playback internally (the
    // JACK timeline keeps rolling linearly), so the honest playhead position is the
    // wrapped musical bar it publishes each cycle, not the raw frame->bar mapping.
    if (songLoopActive())
        return loopedPosition();
    double secs = static_cast<double>(posFrames.load()) / sampleRate();
    return static_cast<float>(timeline->secondsToBar(secs - secsOffsetSecs()));
}

// ── JACK process callback (RT thread) ────────────────────────────────────────

int JackTransport::processCallback(jack_nframes_t nframes, void* arg)
{
    return static_cast<JackTransport*>(arg)->process(nframes);
}

// Index of a port's namedBufs entry; -1 if it has none this cycle.
static int findBuf(const std::vector<std::pair<const std::string*, void*>>& bufs,
                   const char* name)
{
    for (size_t i = 0; i < bufs.size(); i++)
        if (*bufs[i].first == name) return static_cast<int>(i);
    return -1;
}

int JackTransport::process(jack_nframes_t nframes)
{
    cycleStartNs = rtNowNs();
    jack_position_t        pos;
    jack_transport_state_t state = jack_transport_query(client, &pos);
    bool nowPlaying = (state == JackTransportRolling);

    // Collect port buffers, keyed by name for per-channel routing. Each entry
    // points at the stable midiPorts_ key (no string copy); valid for this cycle.
    namedBufs.clear();
    if (midiEnabled && portsMutex.try_lock()) {
        for (auto& [name, port] : midiPorts_) {
            void* b = jack_port_get_buffer(port, nframes);
            jack_midi_clear_buffer(b);
            namedBufs.push_back({&name, b});
        }
        portsMutex.unlock();
    }

    // ── MIDI input (RT thread) ────────────────────────────────────────────────
    // Copy anything that arrived into the sink's lock-free ring, then post at most
    // one wakeup for the whole cycle. jack_midi_event_get() does not allocate and
    // the ring push is a store plus one atomic, so nothing here is RT-unsafe; the
    // single Fl::awake pipe write is what claimWakeup() bounds.
    // The lock is held across the reads (try_lock, so never waited on) so that a
    // port cannot be unregistered from under us mid-cycle.
    if (MidiInputManager* inSink = midiInSink_.load(std::memory_order_acquire)) {
        if (portsMutex.try_lock()) {
            bool any = false;
            for (int slot = 0; slot < kMaxMidiInputs; slot++) {
                jack_port_t* inPort = midiInPorts_[slot];
                if (!inPort) continue;
                void* inBuf = jack_port_get_buffer(inPort, nframes);
                const jack_nframes_t nIn = jack_midi_get_event_count(inBuf);
                for (jack_nframes_t i = 0; i < nIn; i++) {
                    jack_midi_event_t ev;
                    if (jack_midi_event_get(&ev, inBuf, i) != 0) continue;
                    inSink->pushFromRt(slot, ev.buffer, static_cast<int>(ev.size));
                    any = true;
                }
            }
            portsMutex.unlock();
            if (any && inSink->claimWakeup()) inSink->postWakeup();
        }
    }

    events.clear();

    // Raw pending messages (program changes etc.) at frame 0. Queued before the
    // engine renders, so they sort ahead of its frame-0 events.
    if (!namedBufs.empty() && pendingMutex_.try_lock()) {
        for (const auto& pm : pendingMsgs_) {
            const int idx = findBuf(namedBufs, pm.portName);
            if (idx >= 0) events.push(idx, 0, pm.data, pm.len);
        }
        pendingMsgs_.clear();
        pendingMutex_.unlock();
    }

    // A relocate: the frame counter did not land where simply playing on (or standing
    // still) would have put it. Stopped cycles count too — seeking with the transport
    // paused is a jump the engine has to see, otherwise its musical position stays at
    // the frame the user just left. The driver holds on to it until a cycle renders.
    const bool jumped = !firstCall &&
                        (pos.frame != lastFrame + (lastRolling ? nframes : 0));

    // renderCycle() converts the window to musical time, splits it at the song-loop
    // seam when looping, and emits each message; the Sequencer's bar offset shifts
    // frame<->bar so a tempo re-anchor or a Loop -> Song hand-off keeps playback
    // rolling without a reposition (see reanchor() / endLoopMode()).
    const bool wasRolling = wasPlaying;
    runCycle({static_cast<int64_t>(pos.frame), static_cast<int64_t>(nframes),
              nowPlaying, jumped});

    // The driver leaves the events sorted by buffer and then frame. JACK drops any
    // event written out of frame order, so this single ordered pass is what merges
    // the pending / note-off / note-on / param writes.
    for (const auto& e : events)
        jack_midi_event_write(namedBufs[e.port].second, e.frame, e.data, e.len);

    posFrames.store(pos.frame + nframes);
    playing_.store(nowPlaying);

    if ((nowPlaying != wasRolling || jumped) && onTransportEvent)
        onTransportEvent();

    lastFrame   = pos.frame;
    lastRolling = nowPlaying;
    firstCall   = false;

    return 0;
}

// ── CycleDriver hooks (RT thread, inside renderCycle) ─────────────────────────

int JackTransport::resolvePort(const std::string& port)
{
    return findBuf(namedBufs, port.c_str());
}

void JackTransport::emitUnbuffered(const std::string& port, long offset,
                                   const uint8_t* data, int len)
{
    // Not one of our JACK ports: hand it to the scheduler, due when this frame is
    // heard — the cycle's start plus the offset, plus one period, which is when JACK
    // plays out what this cycle writes, so both kinds of port line up.
    if (!scheduler_) return;
    const int64_t dueNs = cycleStartNs +
        static_cast<int64_t>((offset + cycleFrames()) * 1e9 / sampleRate());
    scheduler_->push(MidiScheduler::Producer::Jack, port.c_str(), dueNs, data, len);
}

// ── Port management (UI thread) ───────────────────────────────────────────────

bool JackTransport::addMidiPort(const std::string& name) {
    if (!client || !midiEnabled || name.empty()) return false;
    jack_port_t* p = jack_port_register(client, name.c_str(),
                                         JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (!p) {
        fprintf(stderr, "JackTransport: could not register port '%s'\n", name.c_str());
        return false;
    }
    std::lock_guard<std::mutex> lk(portsMutex);
    midiPorts_[name] = p;
    return true;
}

// ── MIDI input port ───────────────────────────────────────────────────────────

void JackTransport::setMidiInSink(MidiInputManager* s) {
    midiInSink_.store(s, std::memory_order_release);
}

bool JackTransport::addMidiInPort(int slot, const std::string& name) {
    if (!client || !midiEnabled || name.empty()) return false;
    if (slot < 0 || slot >= kMaxMidiInputs) return false;
    // Registers unconditionally and overwrites, as PortRegistry::reregisterJack()
    // does for the outputs: after a server restart `client` is a new one and any
    // handle we are still holding is stale, so keeping the old one would leave the
    // input permanently dead.
    jack_port_t* p = jack_port_register(client, name.c_str(),
                                        JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    if (!p) {
        fprintf(stderr, "JackTransport: could not register input port '%s'\n", name.c_str());
        return false;
    }
    std::lock_guard<std::mutex> lk(portsMutex);
    midiInPorts_[slot] = p;
    return true;
}

bool JackTransport::removeMidiInPort(int slot) {
    if (!client || slot < 0 || slot >= kMaxMidiInputs) return false;
    jack_port_t* p = nullptr;
    {
        std::lock_guard<std::mutex> lk(portsMutex);
        if (!midiInPorts_[slot]) return false;
        p = midiInPorts_[slot];
        midiInPorts_[slot] = nullptr;
    }
    jack_port_unregister(client, p);
    return true;
}

bool JackTransport::renameMidiInPort(int slot, const std::string& newName) {
    if (!client || newName.empty() || slot < 0 || slot >= kMaxMidiInputs) return false;
    std::lock_guard<std::mutex> lk(portsMutex);
    jack_port_t* p = midiInPorts_[slot];
    if (!p) return false;
    if (jack_port_rename(client, p, newName.c_str()) != 0) {
        fprintf(stderr, "JackTransport: could not rename input port -> '%s'\n",
                newName.c_str());
        return false;
    }
    return true;
}

bool JackTransport::removeMidiPort(const std::string& name) {
    if (!client) return false;
    jack_port_t* p = nullptr;
    {
        std::lock_guard<std::mutex> lk(portsMutex);
        auto it = midiPorts_.find(name);
        if (it == midiPorts_.end()) return false;
        p = it->second;
        midiPorts_.erase(it);
    }
    jack_port_unregister(client, p);
    return true;
}

bool JackTransport::renameMidiPort(const std::string& oldName, const std::string& newName) {
    if (!client || newName.empty()) return false;
    std::lock_guard<std::mutex> lk(portsMutex);
    auto it = midiPorts_.find(oldName);
    if (it == midiPorts_.end()) return false;
    jack_port_t* p = it->second;
    if (jack_port_rename(client, p, newName.c_str()) != 0) {
        fprintf(stderr, "JackTransport: could not rename port '%s' -> '%s'\n",
                oldName.c_str(), newName.c_str());
        return false;
    }
    midiPorts_.erase(it);
    midiPorts_[newName] = p;
    return true;
}
