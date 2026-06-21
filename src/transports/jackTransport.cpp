#include "jackTransport.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// ── Construction / destruction ────────────────────────────────────────────────

JackTransport::JackTransport()
{
    // Pre-reserve the RT-thread output buffers so process() never allocates. The
    // bound matches the worst dense window a JACK cycle can produce (param-ramp
    // densification is capped per segment). The Sequencer base pre-reserves its own
    // scratch (activeNotes / paramScratch / resetScratch).
    namedBufs.reserve(64);
    outEvents.reserve(4096);
}

JackTransport::~JackTransport()
{
    if (client && jackAlive.load()) {
        jack_deactivate(client);
        for (auto& [name, port] : midiPorts_)
            jack_port_unregister(client, port);
        jack_client_close(client);
    }
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

    sampleRate = jack_get_sample_rate(client);

    jack_set_process_callback(client, processCallback, this);

    // Detect the server going away without polling: JACK invokes this from one
    // of its own threads, so the thunk only flips an atomic and hands off to the
    // main thread via Fl::awake.
    jack_on_shutdown(client, &JackTransport::shutdownThunk, this);

    if (jack_activate(client) != 0) {
        fprintf(stderr, "JackTransport: could not activate JACK client\n");
        jack_client_close(client);
        client = nullptr;
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
    {
        std::lock_guard<std::mutex> lk(portsMutex);
        midiPorts_.clear();
    }
    jackAlive.store(false);

    // Reset RT-thread state so a reconnected client starts clean. activeNotes is
    // owned by the Sequencer base; a fresh transport drops any sustained notes
    // along with the client, so leave them — the next stop/jump path resets cleanly.
    lastFrame  = 0;
    wasPlaying = false;
    firstCall  = true;
}

// ── Pending raw-MIDI queue (UI thread) ─────────────────────────────────────────

void JackTransport::enqueue(const std::string& portName, const uint8_t* data, int len)
{
    if (!client || !midiEnabled || len < 1 || len > 3) return;
    PendingMsg m{portName, {0, 0, 0}, len};
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
void JackTransport::rewind(){ if (client && jackAlive.load()) jack_transport_locate(client, 0); }

void JackTransport::seek(float bars)
{
    if (!client || !jackAlive.load() || !timeline) return;
    double secs  = timeline->barToSeconds(std::max(0.0f, bars));
    auto   frame = static_cast<jack_nframes_t>(secs * sampleRate);
    jack_transport_locate(client, frame);
}

float JackTransport::position() const
{
    if (!timeline) return 0.0f;
    double secs = static_cast<double>(posFrames.load()) / sampleRate;
    return static_cast<float>(timeline->secondsToBar(secs));
}

// ── JACK process callback (RT thread) ────────────────────────────────────────

int JackTransport::processCallback(jack_nframes_t nframes, void* arg)
{
    return static_cast<JackTransport*>(arg)->process(nframes);
}

// Find the namedBufs buffer for a port; nullptr if not found.
static void* findBuf(const std::vector<std::pair<const std::string*, void*>>& bufs,
                     const char* name)
{
    for (const auto& e : bufs)
        if (*e.first == name) return e.second;
    return nullptr;
}

int JackTransport::process(jack_nframes_t nframes)
{
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

    outEvents.clear();
    curBlockStart = pos.frame;
    curNframes    = nframes;

    // Raw pending messages (program changes etc.) at frame 0.
    if (!namedBufs.empty() && pendingMutex_.try_lock()) {
        for (const auto& pm : pendingMsgs_) {
            void* b = findBuf(namedBufs, pm.portName.c_str());
            if (b) {
                OutEvent e{b, 0, static_cast<uint32_t>(outEvents.size()), {}, pm.len};
                for (int i = 0; i < pm.len && i < 3; i++) e.data[i] = pm.data[i];
                outEvents.push_back(e);
            }
        }
        pendingMsgs_.clear();
        pendingMutex_.unlock();
    }

    bool jumped = !firstCall && wasPlaying && (pos.frame != lastFrame + nframes);

    // Fire events for exactly this buffer: [pos.frame, pos.frame + nframes). JACK
    // advances pos.frame contiguously by nframes when rolling, so each cycle's
    // window abuts the previous one. emit() (below) converts each bar to a frame
    // offset relative to curBlockStart.
    float prevBars = snapSecondsToBar(static_cast<double>(pos.frame) / sampleRate);
    float curBars  = snapSecondsToBar(static_cast<double>(pos.frame + nframes) / sampleRate);

    bool ran = renderWindow(nowPlaying, jumped, prevBars, curBars);

    // Flush the cycle's events in non-decreasing frame order per buffer. JACK
    // drops any event written out of frame order, so this single ordered pass
    // merges the pending / note-off / note-on / param writes. seq is the insertion
    // index, a total-order tiebreaker so the sort can be in-place (no temp-buffer
    // allocation, unlike stable_sort) while keeping same-frame insertion order.
    std::sort(outEvents.begin(), outEvents.end(),
        [](const OutEvent& a, const OutEvent& b) {
            if (a.buf   != b.buf)   return a.buf   < b.buf;
            if (a.frame != b.frame) return a.frame < b.frame;
            return a.seq < b.seq;
        });
    for (const OutEvent& e : outEvents)
        jack_midi_event_write(e.buf, e.frame, e.data, e.len);

    posFrames.store(pos.frame + nframes);
    playing_.store(nowPlaying);

    // Only commit wasPlaying when renderWindow actually ran (it skips on a failed
    // snapMutex try_lock); leaving it stale lets the missed stop/jump retry next cycle.
    if (ran) {
        if ((nowPlaying != wasPlaying || jumped) && onTransportEvent)
            onTransportEvent();
        wasPlaying = nowPlaying;
    }

    lastFrame  = pos.frame;
    firstCall  = false;

    return 0;
}

// ── Sequencer output hook (RT thread, snapMutex held by renderWindow) ─────────

void JackTransport::emit(const std::string& port, float bar,
                          const uint8_t* data, int len)
{
    void* buf = findBuf(namedBufs, port.c_str());
    if (!buf) return;

    long frameAbs = static_cast<long>(snapBarToSeconds(bar) * sampleRate);
    long off      = frameAbs - static_cast<long>(curBlockStart);
    if (off < 0) off = 0;
    if (curNframes > 0 && off > static_cast<long>(curNframes) - 1)
        off = static_cast<long>(curNframes) - 1;

    OutEvent e{buf, static_cast<jack_nframes_t>(off),
               static_cast<uint32_t>(outEvents.size()), {}, len};
    for (int i = 0; i < len && i < 3; ++i)
        e.data[i] = data[i];
    outEvents.push_back(e);
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
