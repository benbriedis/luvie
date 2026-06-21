/* -----------------------------------------------------------------------------
   Luvie LV2 DSP plugin.

   MIDI is produced as a proper LV2 plugin: the DSP has an atom MIDI output port
   (midi_out) and generates MIDI in run(), driven by the host's transport via the
   time:Position atoms on control_in. The host therefore sees the MIDI (meter,
   in-session routing) and playback follows the host's transport whether or not
   the host drives the JACK transport — so it works in Ardour, Carla, Qtractor,
   etc. (The standalone / NSM app keeps its own JACK/native ports via the same
   Sequencer core; only the *output backend* differs.)

   Why the host time and not jack_transport_query(): a normal LV2 host (Ardour)
   runs its own transport and need not roll the JACK transport at all, so the old
   private-JACK-client engine never saw "rolling" and stayed silent even while the
   GUI playhead (fed from time:Position) moved. Driving from time:Position fixes
   that and removes the side-channel JACK client entirely.

   The sequencer uses Luvie's *own* tempo map: time:Position's sample `frame` is
   tempo-independent, so we map frame -> seconds -> Luvie bars through the snapshot,
   exactly as the JACK backend mapped pos.frame. The host's bpm/beat fields are not
   used for note timing.

   State path: the UI sends the full project as one or more luvie_state atoms to
   control_in (chunked so a session bigger than a host buffer still gets through; see
   LuvieStateChunk). run() never parses on the audio thread — it just forwards each
   chunk to the LV2 Worker (work:schedule), and the host runs work() on a non-RT
   thread to reassemble, parse, and apply it to the engine (timeline / instrument
   routing). The worker is the single writer of the snapshot; run() only reads the
   lock-free snapshot. There is no background poll thread and no state-file polling.

   The per-process state file remains only as a one-shot handoff for project
   load/restore: dsp_restore writes it so the UI can repaint the restored project
   when its editor is next opened, and the UI writes it on close for the next open.
   It is never polled.
   ----------------------------------------------------------------------------- */

#include "luvie_dsp.h"
#include <lv2/state/state.h>
#include <lv2/worker/worker.h>

#include "sequencer.hpp"
#include "observableSong.hpp"
#include "timelineIO.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"

/* Opt-in diagnostics: run the host with LUVIE_DEBUG=1 to trace state application
   and per-second transport/MIDI activity on stderr. */
static bool luvieDebug()
{
    static const bool on = [] {
        const char* e = getenv("LUVIE_DEBUG");
        return e && *e && strcmp(e, "0") != 0;
    }();
    return on;
}

/* -----------------------------------------------------------------------
   Lv2Engine — Sequencer whose emit() forges MIDI into the LV2 midi_out port.

   There is a single output port, so port names are irrelevant here (routing is
   by MIDI channel, which the snapshot already carries). Like the JACK backend it
   collects the cycle's events and sorts them by frame before writing, because the
   atom forge (and consumers) require non-decreasing event times.
   ----------------------------------------------------------------------- */
class Lv2Engine : public Sequencer {
public:
    explicit Lv2Engine(double sr) : sampleRate(sr) { evs.reserve(4096); }

    void   setSampleRate(double sr) { sampleRate = sr; }
    double sampleRateHz() const     { return sampleRate; }

    /* Generate this cycle's MIDI into an already-opened forge sequence.
       startSecs is the cycle's start position on Luvie's timeline (frame/sr). */
    void process(LV2_Atom_Forge* fg, const URIs* u,
                 double startSecs, uint32_t nf, bool nowPlaying, bool jumped)
    {
        forge = fg;
        uris  = u;
        cycleStartSecs = startSecs;
        nframes = nf;
        evs.clear();

        float prevBars = snapSecondsToBar(startSecs);
        float curBars  = snapSecondsToBar(startSecs + (double)nf / sampleRate);

        bool ran = renderWindow(nowPlaying, jumped, prevBars, curBars);

        std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
            if (a.frame != b.frame) return a.frame < b.frame;
            return a.seq < b.seq;
        });
        for (const Ev& e : evs) {
            lv2_atom_forge_frame_time(forge, e.frame);
            lv2_atom_forge_atom(forge, e.len, uris->midi_MidiEvent);
            lv2_atom_forge_write(forge, e.data, e.len);
        }
        lastEmitted = (int)evs.size();

        if (ran) wasPlaying = nowPlaying;
    }

    int  lastEmittedCount() const { return lastEmitted; }

    /* Fill bar / barBeat / beatsPerBar for a position on Luvie's own timeline, so
       the UI playhead can be driven from the DSP's authoritative tempo map (the same
       map that times the notes) rather than the host's bar fields. RT-safe: reads the
       snapshot under a try_lock and returns false if a rebuild is in progress (the
       caller then skips authoring a Position this cycle). */
    bool barInfo(double secs, int64_t& bar, float& barBeat, float& bpb)
    {
        if (!snapMutex.try_lock())
            return false;
        float beatsPerBar = 4.0f;
        float barsTotal   = 0.0f;
        if (!snap.segs.empty()) {
            const TimeSegment* seg = &snap.segs.back();
            for (size_t i = 0; i + 1 < snap.segs.size(); i++)
                if (secs < snap.segs[i + 1].startSecs) { seg = &snap.segs[i]; break; }
            double secsPerBar = seg->beatsPerBar * 60.0 / seg->bpm;
            barsTotal   = seg->bar + (float)((secs - seg->startSecs) / secsPerBar);
            beatsPerBar = (float)seg->beatsPerBar;
        }
        snapMutex.unlock();
        if (barsTotal < 0.0f) barsTotal = 0.0f;
        bar     = (int64_t)barsTotal;
        barBeat = (barsTotal - (float)bar) * beatsPerBar;
        bpb     = beatsPerBar;
        return true;
    }

protected:
    void emit(const std::string& /*port*/, float bar,
              const uint8_t* data, int len) override
    {
        long off = static_cast<long>((snapBarToSeconds(bar) - cycleStartSecs) * sampleRate);
        if (off < 0) off = 0;
        if (nframes > 0 && off > static_cast<long>(nframes) - 1)
            off = static_cast<long>(nframes) - 1;

        Ev e{static_cast<uint32_t>(off), static_cast<uint32_t>(evs.size()), {}, len};
        for (int i = 0; i < len && i < 3; ++i) e.data[i] = data[i];
        evs.push_back(e);
    }

private:
    LV2_Atom_Forge* forge = nullptr;
    const URIs*     uris  = nullptr;
    double          sampleRate     = 48000.0;
    double          cycleStartSecs = 0.0;
    uint32_t        nframes        = 0;
    int             lastEmitted    = 0;

    struct Ev { uint32_t frame; uint32_t seq; uint8_t data[3]; int len; };
    std::vector<Ev> evs;   // reused across cycles; pre-reserved in ctor
};

/* -----------------------------------------------------------------------
   Plugin instance
   ----------------------------------------------------------------------- */

struct Plugin {
    LV2_URID_Map*  map = nullptr;
    LV2_Log_Logger logger{};
    LV2_Atom_Forge forge{};
    URIs           uris{};

    const LV2_Atom_Sequence* controlIn = nullptr;
    /* Single atom output: MIDI events (host -> instrument) plus our own
       time:Position (UI playhead) and state:StateChanged (host dirty flag). */
    LV2_Atom_Sequence*       out       = nullptr;

    ObservableSong* song   = nullptr;
    Lv2Engine*      engine = nullptr;

    /* Transport position tracking. The host sends time:Position only on transport
       changes, so between updates we advance curFrame ourselves while playing. */
    int64_t curFrame = 0;
    bool    playing  = false;

    /* LV2 Worker: run() hands the incoming JSON blob to schedule_work(); the host
       runs work() on a non-RT thread, which parses it and applies it to the engine.
       The worker thread is the ONLY thread that mutates song/engine state (single
       writer), exactly as the old poll thread was. */
    LV2_Worker_Schedule* schedule = nullptr;

    /* Set by the worker after applying state; read+cleared by run() to emit a
       state:StateChanged so the host marks its project dirty. */
    std::atomic<bool> stateDirty{false};

    /* JSON state for LV2 save/restore. Guarded because dsp_save (host thread) and
       the worker / dsp_restore both touch it. */
    std::mutex  stateMutex;
    std::string stateJson;

    std::string stateFilePath;

    /* Chunk reassembly — worker-thread-only state. run() forwards each incoming
       luvie_state chunk to the worker in order; work() appends payloads here until a
       message is complete, then applies it. */
    std::string rxAccum;
    uint32_t    rxMsgId         = 0;
    uint32_t    rxExpectedOffset = 0;
    bool        rxActive        = false;
};

/* -----------------------------------------------------------------------
   State application (worker thread, or the host thread during dsp_restore)
   ----------------------------------------------------------------------- */

static void applyState(Plugin* p, const std::string& json)
{
    AppState st;
    if (!appStateFromJsonString(json, st))
        return;
    /* A valid session always has at least one track; an empty timeline is a
       stale/uninitialised file and applying it would wipe the engine. */
    if (st.timeline.tracks.empty()) {
        if (luvieDebug())
            fprintf(stderr, "[luvie] applyState: ignoring state with 0 tracks\n");
        return;
    }

    if (luvieDebug()) {
        size_t notes = 0;
        for (const auto& pat : st.timeline.patterns)
            notes += pat.notes.size() + pat.drumNotes.size();
        fprintf(stderr, "[luvie] applyState: %zu tracks, %zu patterns, %zu notes, "
                "%zu instruments\n", st.timeline.tracks.size(),
                st.timeline.patterns.size(), notes, st.jackInstruments.size());
    }

    /* Instrument routing (id -> channel + program/bank). Port names are kept for
       parity with the standalone routing but are irrelevant on the single LV2
       MIDI port; output is routed purely by MIDI channel. */
    std::vector<Sequencer::InstrumentRouting> routings;
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

/* -----------------------------------------------------------------------
   LV2 Worker: parse + apply state off the audio thread
   ----------------------------------------------------------------------- */

/* Runs on the host's non-RT worker thread, once per chunk run() scheduled, in order.
   `data` is a LuvieStateChunk header + payload that the host has already copied, so
   reassembling/parsing/allocating here is safe and run() is never blocked. Chunks
   are accumulated until the message is complete; applyState() then rebuilds the
   snapshot under its own lock and flags stateDirty for run() to emit StateChanged.

   A gap (a dropped chunk → msgId/offset mismatch) resets reassembly and waits for
   the next message's first chunk (offset 0); this is safe because every UI edit
   sends the complete current project, so the next send recovers. */
static LV2_Worker_Status work(
    LV2_Handle                  instance,
    LV2_Worker_Respond_Function /*respond*/,
    LV2_Worker_Respond_Handle   /*handle*/,
    uint32_t                    size,
    const void*                 data)
{
    Plugin* p = (Plugin*)instance;
    if (!data || size < sizeof(LuvieStateChunk))
        return LV2_WORKER_SUCCESS;

    LuvieStateChunk hdr;
    std::memcpy(&hdr, data, sizeof(hdr));   // data may be unaligned
    const char* payload = (const char*)data + sizeof(LuvieStateChunk);
    uint32_t avail = size - (uint32_t)sizeof(LuvieStateChunk);
    uint32_t chunkSize = hdr.chunkSize < avail ? hdr.chunkSize : avail;

    if (hdr.offset == 0) {
        /* First chunk of a new message: start fresh. */
        p->rxMsgId          = hdr.msgId;
        p->rxExpectedOffset = 0;
        p->rxActive         = true;
        p->rxAccum.clear();
        p->rxAccum.reserve(hdr.totalSize);
    }
    if (!p->rxActive || hdr.msgId != p->rxMsgId || hdr.offset != p->rxExpectedOffset) {
        p->rxActive = false;                // out of sync; wait for next message
        return LV2_WORKER_SUCCESS;
    }

    p->rxAccum.append(payload, chunkSize);
    p->rxExpectedOffset += chunkSize;

    if (p->rxExpectedOffset >= hdr.totalSize) {
        applyState(p, p->rxAccum);
        p->rxActive = false;
        p->rxAccum.clear();
        p->rxAccum.shrink_to_fit();         // don't hold a whole session between edits
    }
    return LV2_WORKER_SUCCESS;
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
    uris->atom_Long          = map->map(map->handle, LV2_ATOM__Long);
    uris->atom_Int           = map->map(map->handle, LV2_ATOM__Int);
    uris->atom_Float         = map->map(map->handle, LV2_ATOM__Float);
    uris->time_Position      = map->map(map->handle, LV2_TIME__Position);
    uris->time_bar           = map->map(map->handle, LV2_TIME__bar);
    uris->time_barBeat       = map->map(map->handle, LV2_TIME__barBeat);
    uris->time_beatsPerBar   = map->map(map->handle, LV2_TIME__beatsPerBar);
    uris->time_beatsPerMinute= map->map(map->handle, LV2_TIME__beatsPerMinute);
    uris->time_speed         = map->map(map->handle, LV2_TIME__speed);
    uris->time_frame         = map->map(map->handle, LV2_TIME__frame);
    uris->midi_MidiEvent     = map->map(map->handle, LV2_MIDI__MidiEvent);
    uris->luvie_state        = map->map(map->handle, LUVIE_STATE_URI);
    uris->state_StateChanged = map->map(map->handle, LV2_STATE__StateChanged);
}

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sampleRate,
    const char* bundlePath,
    const LV2_Feature* const* features)
{
    (void)descriptor; (void)bundlePath;

    Plugin* p = new (std::nothrow) Plugin;
    if (!p)
        return nullptr;

    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,        &p->logger.log, false,
        LV2_URID__map,       &p->map,        true,
        LV2_WORKER__schedule, &p->schedule,  true,
        NULL);
    lv2_log_logger_set_map(&p->logger, p->map);
    if (missing) {
        lv2_log_error(&p->logger, "Missing feature <%s>\n", missing);
        delete p;
        return nullptr;
    }

    mapURIs(p->map, &p->uris);
    lv2_atom_forge_init(&p->forge, p->map);

    /* Per-process path: the one-shot load/restore handoff to the UI (same PID — both
       load into the host). Never polled; see file header. */
    char buf[256];
    snprintf(buf, sizeof(buf), "/tmp/luvie_state_%d.json", (int)getpid());
    p->stateFilePath = buf;

    p->song   = new ObservableSong(120.0f, 4, 4);
    p->engine = new Lv2Engine(sampleRate);
    p->engine->setTimeline(p->song);

    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Plugin* p = (Plugin*)instance;
    switch (port) {
        case PORT_CONTROL_IN: p->controlIn = (const LV2_Atom_Sequence*)data; break;
        case PORT_OUT:        p->out       = (LV2_Atom_Sequence*)data;       break;
    }
}

static void activate(LV2_Handle instance)   { (void)instance; }
static void deactivate(LV2_Handle instance) { (void)instance; }

/* Read an integer-valued atom (Int or Long) into out; returns true if recognised. */
static bool atomToI64(const URIs* uris, const LV2_Atom* a, int64_t& out)
{
    if (!a) return false;
    if (a->type == uris->atom_Long) { out = ((const LV2_Atom_Long*)a)->body; return true; }
    if (a->type == uris->atom_Int)  { out = ((const LV2_Atom_Int*)a)->body;  return true; }
    return false;
}

static void run(LV2_Handle instance, uint32_t sample_count)
{
    Plugin* p = (Plugin*)instance;
    const URIs* uris = &p->uris;

    /* ── Read host transport + UI state chunks from control_in ───────────────────
       time:Position coalesces to the latest (only the newest matters); luvie_state
       chunks must each be forwarded to the worker, in order, so it can reassemble a
       multi-chunk project. We never parse on this (RT) thread. */
    bool jumped = false;
    const LV2_Atom_Object* lastPos = nullptr;
    int ctrlEvents = 0;   // raw events seen on control_in this cycle (diagnostics)
    if (p->controlIn) {
        LV2_ATOM_SEQUENCE_FOREACH(p->controlIn, ev) {
            ctrlEvents++;
            if (ev->body.type == uris->luvie_state) {
                if (p->schedule) {
                    LV2_Worker_Status ws = p->schedule->schedule_work(
                        p->schedule->handle, ev->body.size, LV2_ATOM_BODY_CONST(&ev->body));
                    if (ws != LV2_WORKER_SUCCESS && luvieDebug())
                        fprintf(stderr, "[luvie] schedule_work rejected %u-byte chunk "
                                "(status %d); host worker buffer too small?\n",
                                ev->body.size, (int)ws);
                }
            } else if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
                const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
                if (obj->body.otype == uris->time_Position)
                    lastPos = obj;
            }
        }
    }
    if (lastPos) {
        const LV2_Atom* frameAtom = nullptr;
        const LV2_Atom* speedAtom = nullptr;
        lv2_atom_object_get(lastPos,
            uris->time_frame, &frameAtom,
            uris->time_speed, &speedAtom,
            0);

        int64_t hostFrame = p->curFrame;
        if (atomToI64(uris, frameAtom, hostFrame)) {
            if (hostFrame != p->curFrame) jumped = true;   // relocate
            p->curFrame = hostFrame;
        }
        if (speedAtom && speedAtom->type == uris->atom_Float)
            p->playing = ((const LV2_Atom_Float*)speedAtom)->body != 0.0f;
    }

    /* ── Single output port: one atom sequence carrying, in frame order:
         1. our own time:Position (frame 0) for the UI playhead,
         2. state:StateChanged (frame 0) for the host dirty flag,
         3. this cycle's MIDI events (frame >= 0) for the host -> instrument.
       Hosts route the MIDI to the instrument and ignore the rest; the UI reads the
       Position and ignores the MIDI. Events stay in non-decreasing frame order
       because the frame-0 objects are written before the (>= 0) MIDI events. ──── */
    if (p->out) {
        LV2_Atom_Forge_Frame seqFrame;
        const uint32_t cap = p->out->atom.size;
        lv2_atom_sequence_clear(p->out);
        p->out->atom.type = uris->atom_Sequence;
        lv2_atom_forge_set_buffer(&p->forge, (uint8_t*)p->out, cap);
        lv2_atom_forge_sequence_head(&p->forge, &seqFrame, 0);

        /* Author our own Position from the DSP's authoritative state (curFrame +
           Luvie's tempo map) rather than forwarding the host's object: this keeps the
           UI playhead in lock-step with the notes, and always carries bar/barBeat so
           the UI never has to depend on which fields the host happened to populate. */
        int64_t bar = 0; float barBeat = 0.0f, bpb = 4.0f;
        double posSecs = (double)p->curFrame / (double)p->engine->sampleRateHz();
        if (p->engine->barInfo(posSecs, bar, barBeat, bpb)) {
            LV2_Atom_Forge_Frame posFrame;
            lv2_atom_forge_frame_time(&p->forge, 0);
            lv2_atom_forge_object(&p->forge, &posFrame, 0, uris->time_Position);
            lv2_atom_forge_key(&p->forge, uris->time_frame);
            lv2_atom_forge_long(&p->forge, p->curFrame);
            lv2_atom_forge_key(&p->forge, uris->time_speed);
            lv2_atom_forge_float(&p->forge, p->playing ? 1.0f : 0.0f);
            lv2_atom_forge_key(&p->forge, uris->time_bar);
            lv2_atom_forge_long(&p->forge, bar);
            lv2_atom_forge_key(&p->forge, uris->time_barBeat);
            lv2_atom_forge_float(&p->forge, barBeat);
            lv2_atom_forge_key(&p->forge, uris->time_beatsPerBar);
            lv2_atom_forge_float(&p->forge, bpb);
            lv2_atom_forge_pop(&p->forge, &posFrame);
        }

        if (p->stateDirty.exchange(false)) {
            LV2_Atom_Forge_Frame objFrame;
            lv2_atom_forge_frame_time(&p->forge, 0);
            lv2_atom_forge_object(&p->forge, &objFrame, 0, uris->state_StateChanged);
            lv2_atom_forge_pop(&p->forge, &objFrame);
        }

        double startSecs = (double)p->curFrame / (double)p->engine->sampleRateHz();
        p->engine->process(&p->forge, uris, startSecs, sample_count, p->playing, jumped);

        lv2_atom_forge_pop(&p->forge, &seqFrame);
    }

    /* Advance our own frame cursor while playing (host only sends position on change). */
    if (p->playing)
        p->curFrame += sample_count;

    if (luvieDebug()) {
        static int dbgAccum = 0;
        dbgAccum += (int)sample_count;
        int emitted = p->engine ? p->engine->lastEmittedCount() : 0;
        if (emitted > 0 || dbgAccum >= (int)p->engine->sampleRateHz()) {
            fprintf(stderr, "[luvie] run: ctrlIn=%s ctrlEvents=%d gotPos=%d playing=%d "
                    "frame=%ld emitted=%d out=%s\n",
                    p->controlIn ? "connected" : "NULL", ctrlEvents,
                    lastPos != nullptr, p->playing, (long)p->curFrame, emitted,
                    p->out ? "connected" : "NULL");
            dbgAccum = 0;
        }
    }
}

static void cleanup(LV2_Handle instance)
{
    Plugin* p = (Plugin*)instance;
    delete p->engine;
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

    /* Restore runs on the host thread at load time with no concurrent worker, so
       apply directly (applyState also stores stateJson for a later dsp_save). */
    applyState(p, std::string((const char*)data, jsonLen));

    /* Also write the JSON to the state file so the UI repaints this project when its
       editor is next opened. A plain write is fine: no concurrent writer at load. */
    if (FILE* f = fopen(p->stateFilePath.c_str(), "wb")) {
        fwrite(data, 1, jsonLen, f);
        fclose(f);
    }

    return LV2_STATE_SUCCESS;
}

static const void* extension_data(const char* uri)
{
    static const LV2_State_Interface  state_iface  = { dsp_save, dsp_restore };
    static const LV2_Worker_Interface worker_iface = { work, nullptr, nullptr };
    if (!strcmp(uri, LV2_STATE__interface))
        return &state_iface;
    if (!strcmp(uri, LV2_WORKER__interface))
        return &worker_iface;
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
