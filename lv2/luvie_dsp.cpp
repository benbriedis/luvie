/* -----------------------------------------------------------------------------
   Luvie LV2 DSP plugin.

   The real-time MIDI engine (a JackTransport with its own JACK client, MIDI
   output ports and sequencing) lives HERE, in the DSP plugin, not in the UI.
   The DSP's lifetime is the plugin's lifetime in the host, so the JACK node and
   its MIDI output persist whether or not the editor window is open — closing the
   editor no longer stops playback or makes the node vanish from the JACK graph.

   How state reaches the engine without touching the real-time thread:
     - The UI writes the full project as JSON to a per-process state file
       (/tmp/luvie_state_<pid>.json) atomically on every change.
     - A background poll thread here watches that file and, when it changes,
       parses it and applies it to the engine (loadTimeline / port registration /
       instrument routing). All of that allocates and calls JACK setup functions,
       so it must stay off both real-time threads:
         * the LV2 run() audio thread (Carla's), and
         * the JACK process callback (which only consumes the lock-free snapshot).
     - run() therefore does no engine work; it just forwards host time:Position to
       the UI and emits state:StateChanged when the poll thread flags a change.

   Transport sync needs no code: JackTransport::process() reads the transport via
   jack_transport_query(), so the engine follows whatever drives JACK transport
   (Carla, in JACK-transport mode) automatically.
   ----------------------------------------------------------------------------- */

#include "luvie_dsp.h"
#include <lv2/state/state.h>

#include "jackTransport.hpp"
#include "observableSong.hpp"
#include "timelineIO.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"

namespace fs = std::filesystem;

/* -----------------------------------------------------------------------
   Plugin instance
   ----------------------------------------------------------------------- */

struct Plugin {
    /* LV2 boilerplate */
    LV2_URID_Map*  map = nullptr;
    LV2_Log_Logger logger{};
    LV2_Atom_Forge forge{};
    URIs           uris{};

    const LV2_Atom_Sequence* controlIn = nullptr;
    LV2_Atom_Sequence*       notifyOut = nullptr;

    /* Real-time MIDI engine — owns its own JACK client + output ports. */
    ObservableSong* song   = nullptr;
    JackTransport*  engine = nullptr;

    /* Background poll thread: watches the state file and applies it to the engine.
       It is the ONLY thread that mutates song/engine state (single writer), which
       is the threading contract JackTransport's UI-thread setters expect. */
    std::thread       pollThread;
    std::atomic<bool> stopPoll{false};

    /* Set by the poll thread after it applies a new state; read+cleared by run()
       to emit a state:StateChanged so the host marks its project dirty. */
    std::atomic<bool> stateDirty{false};

    /* JSON state for LV2 save/restore. Guarded because dsp_save (host thread) and
       the poll thread / dsp_restore both touch it. Includes a trailing '\0'. */
    std::mutex  stateMutex;
    std::string stateJson;

    std::string stateFilePath;

    /* Poll-thread-only bookkeeping. */
    std::set<std::string> registeredPorts;   // JACK MIDI ports currently registered
    fs::file_time_type    lastWrite{};
    uintmax_t             lastSize = 0;
    bool                  haveLast = false;
};

/* -----------------------------------------------------------------------
   State application (poll thread only)
   ----------------------------------------------------------------------- */

static void applyState(Plugin* p, const std::string& json)
{
    AppState st;
    if (!appStateFromJsonString(json, st))
        return;
    /* A valid session always has at least one track; an empty timeline is a
       stale/uninitialised file and applying it would wipe the engine. */
    if (st.timeline.tracks.empty())
        return;

    /* Reconcile JACK MIDI output ports. In plugin mode every output is JACK-backed
       (the UI defaults new ports to the Jack backend); ignore any others. */
    std::set<std::string> desired;
    for (const auto& o : st.jackOutputs)
        if (o.backend == MidiBackend::Jack && !o.portName.empty())
            desired.insert(o.portName);

    for (auto it = p->registeredPorts.begin(); it != p->registeredPorts.end(); ) {
        if (!desired.count(*it)) {
            p->engine->removeMidiPort(*it);
            it = p->registeredPorts.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& name : desired)
        if (!p->registeredPorts.count(name) && p->engine->addMidiPort(name))
            p->registeredPorts.insert(name);

    /* Instrument routing (id → port + channel + program/bank). */
    std::vector<JackTransport::InstrumentRouting> routings;
    routings.reserve(st.jackInstruments.size());
    for (const auto& ci : st.jackInstruments)
        routings.push_back({ci.id, ci.portName, ci.midiChannel,
                            ci.programNumber, ci.bankMsb, ci.bankLsb});
    p->engine->setInstruments(routings);
    p->engine->setNoteParams(st.rootPitch, st.chordType);

    /* Timeline last: each setter rebuilds the RT snapshot, so loading the timeline
       after the routing/params are in place gives a final snapshot with them. */
    p->song->loadTimeline(st.timeline);

    {
        std::lock_guard<std::mutex> lk(p->stateMutex);
        p->stateJson = json;
    }
    p->stateDirty.store(true);
}

/* Read the whole state file into a string; returns false if it can't be read. */
static bool readFile(const std::string& path, std::string& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    bool ok = false;
    if (sz > 0) {
        out.resize((size_t)sz);
        ok = fread(out.data(), 1, (size_t)sz, f) == (size_t)sz;
    }
    fclose(f);
    return ok;
}

static void pollLoop(Plugin* p)
{
    using namespace std::chrono_literals;
    while (!p->stopPoll.load()) {
        /* Try to bring the engine up if a JACK server is (now) available. This also
           covers reconnect after the server went away: close() frees the zombie
           client and drops its (now-invalid) ports, and we clear our own port
           bookkeeping so the forced reload below re-registers everything on the
           fresh client. */
        if (!p->engine->isOpen()) {
            p->engine->close();
            p->registeredPorts.clear();
            if (p->engine->open("Luvie", /*enableMidi=*/true)) {
                p->engine->setTimeline(p->song);
                p->haveLast = false;   // force re-apply to register ports
            }
        }

        std::error_code ec;
        if (fs::exists(p->stateFilePath, ec)) {
            auto when = fs::last_write_time(p->stateFilePath, ec);
            auto size = fs::file_size(p->stateFilePath, ec);
            if (!ec && (!p->haveLast || when != p->lastWrite || size != p->lastSize)) {
                std::string json;
                if (readFile(p->stateFilePath, json)) {
                    applyState(p, json);
                    p->lastWrite = when;
                    p->lastSize  = size;
                    p->haveLast  = true;
                }
            }
        }

        std::this_thread::sleep_for(100ms);
    }
}

/* -----------------------------------------------------------------------
   LV2 callbacks
   ----------------------------------------------------------------------- */

static void mapURIs(LV2_URID_Map* map, URIs* uris)
{
    uris->atom_Blank         = map->map(map->handle, LV2_ATOM__Blank);
    uris->atom_Object        = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_Sequence      = map->map(map->handle, LV2_ATOM__Sequence);
    uris->atom_URID          = map->map(map->handle, LV2_ATOM__URID);
    uris->atom_Chunk         = map->map(map->handle, LV2_ATOM__Chunk);
    uris->time_Position      = map->map(map->handle, LV2_TIME__Position);
    uris->time_bar           = map->map(map->handle, LV2_TIME__bar);
    uris->time_barBeat       = map->map(map->handle, LV2_TIME__barBeat);
    uris->time_beatsPerBar   = map->map(map->handle, LV2_TIME__beatsPerBar);
    uris->time_beatsPerMinute= map->map(map->handle, LV2_TIME__beatsPerMinute);
    uris->time_speed         = map->map(map->handle, LV2_TIME__speed);
    uris->luvie_state        = map->map(map->handle, LUVIE_STATE_URI);
    uris->state_StateChanged = map->map(map->handle, LV2_STATE__StateChanged);
}

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sampleRate,
    const char* bundlePath,
    const LV2_Feature* const* features)
{
    (void)descriptor; (void)bundlePath; (void)sampleRate;

    Plugin* p = new (std::nothrow) Plugin;
    if (!p)
        return nullptr;

    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,  &p->logger.log, false,
        LV2_URID__map, &p->map,        true,
        NULL);
    lv2_log_logger_set_map(&p->logger, p->map);
    if (missing) {
        lv2_log_error(&p->logger, "Missing feature <%s>\n", missing);
        delete p;
        return nullptr;
    }

    mapURIs(p->map, &p->uris);
    lv2_atom_forge_init(&p->forge, p->map);

    /* Same per-process path the UI writes to (same PID — both load into the host). */
    char buf[256];
    snprintf(buf, sizeof(buf), "/tmp/luvie_state_%d.json", (int)getpid());
    p->stateFilePath = buf;

    /* Engine. JackTransport::open() is attempted on the poll thread (it polls for
       a JACK server), so the plugin instantiates fine even if JACK isn't up yet. */
    p->song   = new ObservableSong(120.0f, 4, 4);
    p->engine = new JackTransport();
    JackTransport::silenceLogging();   /* keep the host's console clean */

    p->stopPoll.store(false);
    p->pollThread = std::thread(pollLoop, p);

    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Plugin* p = (Plugin*)instance;
    switch (port) {
        case PORT_CONTROL_IN: p->controlIn = (const LV2_Atom_Sequence*)data; break;
        case PORT_NOTIFY_OUT: p->notifyOut = (LV2_Atom_Sequence*)data;       break;
    }
}

static void activate(LV2_Handle instance)   { (void)instance; }
static void deactivate(LV2_Handle instance) { (void)instance; }

static void run(LV2_Handle instance, uint32_t sample_count)
{
    (void)sample_count;
    Plugin* p = (Plugin*)instance;
    const URIs* uris = &p->uris;

    /* Set up the forge to write into notifyOut. */
    const bool hasNotify = (p->notifyOut != nullptr);
    LV2_Atom_Forge_Frame notifyFrame;
    if (hasNotify) {
        const uint32_t notifyCapacity = p->notifyOut->atom.size;
        lv2_atom_sequence_clear(p->notifyOut);
        p->notifyOut->atom.type = uris->atom_Sequence;
        lv2_atom_forge_set_buffer(&p->forge,
                                  (uint8_t*)p->notifyOut,
                                  sizeof(LV2_Atom) + notifyCapacity);
        lv2_atom_forge_sequence_head(&p->forge, &notifyFrame, 0);
    }

    /* Forward host time:Position atoms to the UI (for its playhead display). The
       engine itself follows JACK transport directly and needs nothing from here. */
    if (p->controlIn) {
        LV2_ATOM_SEQUENCE_FOREACH(p->controlIn, ev) {
            if ((ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank)
                && hasNotify) {
                const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
                if (obj->body.otype == uris->time_Position) {
                    lv2_atom_forge_frame_time(&p->forge, ev->time.frames);
                    lv2_atom_forge_write(&p->forge, &ev->body,
                                         sizeof(LV2_Atom) + ev->body.size);
                }
            }
        }
    }

    /* The poll thread applied a fresh project state; tell the host once so it
       marks the project dirty and re-saves us. The body-less object's type is the
       signal. */
    if (hasNotify && p->stateDirty.exchange(false)) {
        LV2_Atom_Forge_Frame objFrame;
        lv2_atom_forge_frame_time(&p->forge, 0);
        lv2_atom_forge_object(&p->forge, &objFrame, 0, uris->state_StateChanged);
        lv2_atom_forge_pop(&p->forge, &objFrame);
    }

    if (hasNotify)
        lv2_atom_forge_pop(&p->forge, &notifyFrame);
}

static void cleanup(LV2_Handle instance)
{
    Plugin* p = (Plugin*)instance;

    p->stopPoll.store(true);
    if (p->pollThread.joinable())
        p->pollThread.join();

    delete p->engine;   /* closes the JACK client + drops its MIDI ports */
    delete p->song;
    delete p;
}

/* -----------------------------------------------------------------------
   LV2 state save / restore
   ----------------------------------------------------------------------- */

static LV2_State_Status dsp_save(
    LV2_Handle                 instance,
    LV2_State_Store_Function   store,
    LV2_State_Handle           handle,
    uint32_t                   flags,
    const LV2_Feature* const*  features)
{
    (void)flags; (void)features;
    Plugin* p = (Plugin*)instance;

    std::lock_guard<std::mutex> lk(p->stateMutex);
    if (p->stateJson.empty())
        return LV2_STATE_SUCCESS;

    /* Store with a trailing '\0' so the value is a usable C string on restore. */
    return store(handle,
                 p->uris.luvie_state,
                 p->stateJson.c_str(),
                 (uint32_t)p->stateJson.size() + 1,
                 p->uris.atom_Chunk,
                 LV2_STATE_IS_POD);
}

static LV2_State_Status dsp_restore(
    LV2_Handle                    instance,
    LV2_State_Retrieve_Function   retrieve,
    LV2_State_Handle              handle,
    uint32_t                      flags,
    const LV2_Feature* const*     features)
{
    (void)flags; (void)features;
    Plugin* p = (Plugin*)instance;

    size_t   size     = 0;
    uint32_t type     = 0;
    uint32_t valflags = 0;
    const void* data = retrieve(handle, p->uris.luvie_state, &size, &type, &valflags);
    if (!data || type != p->uris.atom_Chunk || size == 0)
        return LV2_STATE_SUCCESS;

    /* The stored value carries a trailing '\0'; the JSON is size-1 bytes. */
    size_t jsonLen = size > 0 ? size - 1 : 0;
    {
        std::lock_guard<std::mutex> lk(p->stateMutex);
        p->stateJson.assign((const char*)data, jsonLen);
    }

    /* Write the JSON to the state file so the poll thread applies it to the engine
       and the UI restores from it when it next opens. Restore runs at load time
       with no concurrent writer, so a plain write is sufficient. */
    if (FILE* f = fopen(p->stateFilePath.c_str(), "wb")) {
        fwrite(data, 1, jsonLen, f);
        fclose(f);
    }

    return LV2_STATE_SUCCESS;
}

static const void* extension_data(const char* uri)
{
    static const LV2_State_Interface state_iface = { dsp_save, dsp_restore };
    if (!strcmp(uri, LV2_STATE__interface))
        return &state_iface;
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    LUVIE_DSP_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

extern "C" LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : nullptr;
}
