// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

// Minimal LV2 host harness: dlopen luvie_dsp.so, feed it the project state (as a
// luvie_state atom on control_in) + a time:Position stream, and dump whatever it
// forges onto midi_out. Used to isolate "DSP isn't emitting" from "Ardour isn't
// routing".
//
// Mirrors the real data path: the UI sends the project as one JSON-blob atom on
// control_in, the DSP hands it to the LV2 Worker, and work() parses + applies it.
// This harness provides a minimal work:schedule that runs work() synchronously.
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <lv2/midi/midi.h>
#include <lv2/worker/worker.h>

#include "luvie_dsp.h"   // LuvieStateChunk

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"
#define LUVIE_LOOP_URI  "https://github.com/benbriedis/luvie#LoopState"

static std::map<std::string,LV2_URID> g_uris;
static std::vector<std::string>       g_rev{ "" };  // index 0 unused

static LV2_URID map_uri(LV2_URID_Map_Handle, const char* uri) {
    auto it = g_uris.find(uri);
    if (it != g_uris.end()) return it->second;
    LV2_URID id = (LV2_URID)g_rev.size();
    g_uris[uri] = id;
    g_rev.push_back(uri);
    return id;
}

// Minimal worker. A real host runs work() on its own non-RT thread, so the effect of
// a scheduled message lands some cycles after run() asked for it. Running it inline
// (g_workerDelay == 0) hides that latency completely — and the Loop -> Song hand-off
// is exactly the case where the latency is the thing being measured — so the delay is
// settable: payloads are queued and drained g_workerDelay cycles later, in order.
static const LV2_Worker_Interface* g_worker      = nullptr;
static LV2_Handle                  g_inst        = nullptr;
static int                         g_workerDelay = 0;

struct PendingWork { int dueCycle; std::vector<uint8_t> data; };
static std::vector<PendingWork> g_pendingWork;
static int                      g_cycle = 0;

static LV2_Worker_Status test_respond(LV2_Worker_Respond_Handle, uint32_t, const void*) {
    return LV2_WORKER_SUCCESS;
}
static LV2_Worker_Status test_schedule(LV2_Worker_Schedule_Handle, uint32_t size, const void* data) {
    if (!g_worker || !g_worker->work) return LV2_WORKER_SUCCESS;
    if (g_workerDelay <= 0) {
        g_worker->work(g_inst, test_respond, nullptr, size, data);
        return LV2_WORKER_SUCCESS;
    }
    // The host copies the payload before handing it to the worker thread; so do we.
    PendingWork w{ g_cycle + g_workerDelay, {} };
    w.data.assign((const uint8_t*)data, (const uint8_t*)data + size);
    g_pendingWork.push_back(std::move(w));
    return LV2_WORKER_SUCCESS;
}

// Drain everything due at or before this cycle, before run() is called for it.
static void drainWork(int cycle) {
    for (size_t i = 0; i < g_pendingWork.size(); ) {
        if (g_pendingWork[i].dueCycle <= cycle) {
            g_worker->work(g_inst, test_respond, nullptr,
                           (uint32_t)g_pendingWork[i].data.size(),
                           g_pendingWork[i].data.data());
            g_pendingWork.erase(g_pendingWork.begin() + i);
        } else {
            ++i;
        }
    }
}

// Forge one luvie_loop atom: LuvieStateChunk(msgId 0) + LuvieLoopState + entries.
// Assembled in a flat buffer and written in a single call, because
// lv2_atom_forge_write() pads each write to 8 bytes — three separate writes would
// slip 4 bytes of padding between the header and the entries, and the DSP (which
// reads them at their packed offsets) would then see garbage flags. That is exactly
// what used to make --loop switch on Loop Mode with nothing active.
static void forgeLoopAtom(LV2_Atom_Forge* forge, LV2_URID uLoop,
                          const LuvieLoopState& ls,
                          const LuvieLoopEntry* entries, size_t count) {
    const uint32_t payload = (uint32_t)(sizeof(ls) + count * sizeof(LuvieLoopEntry));
    LuvieStateChunk mark{ 0, payload, 0, 0 };
    std::vector<uint8_t> body(sizeof(mark) + payload);
    uint8_t* p = body.data();
    memcpy(p, &mark, sizeof(mark)); p += sizeof(mark);
    memcpy(p, &ls,   sizeof(ls));   p += sizeof(ls);
    for (size_t i = 0; i < count; i++, p += sizeof(LuvieLoopEntry))
        memcpy(p, &entries[i], sizeof(LuvieLoopEntry));
    lv2_atom_forge_frame_time(forge, 0);
    lv2_atom_forge_atom(forge, (uint32_t)body.size(), uLoop);
    lv2_atom_forge_write(forge, body.data(), (uint32_t)body.size());
}

int main(int argc, char** argv) {
    const char* so    = argc > 1 ? argv[1] : "build/luvie.lv2/luvie_dsp.so";
    const char* state = argc > 2 ? argv[2] : "/tmp/luvie_state_240663.json";
    const double sr   = 48000.0;
    const uint32_t nframes = 256;

    // Read the project state into memory; we send it as an atom, not via a file.
    std::string json;
    {
        FILE* in = fopen(state, "rb"); if (!in) { perror("state"); return 1; }
        char b[4096]; size_t n;
        while ((n = fread(b,1,sizeof b,in))>0) json.append(b, n);
        fclose(in);
    }
    printf("read %zu bytes of state from %s\n", json.size(), state);

    void* h = dlopen(so, RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    auto desc_fn = (const LV2_Descriptor*(*)(uint32_t))dlsym(h, "lv2_descriptor");
    if (!desc_fn) { fprintf(stderr, "no lv2_descriptor\n"); return 1; }
    const LV2_Descriptor* d = desc_fn(0);
    printf("plugin URI: %s\n", d->URI);

    LV2_URID_Map map{ nullptr, map_uri };
    LV2_Feature mapF{ LV2_URID__map, &map };
    LV2_Worker_Schedule sched{ nullptr, test_schedule };
    LV2_Feature workerF{ LV2_WORKER__schedule, &sched };
    const LV2_Feature* features[] = { &mapF, &workerF, nullptr };

    LV2_Handle inst = d->instantiate(d, sr, "build/luvie.lv2/", features);
    if (!inst) { fprintf(stderr, "instantiate failed\n"); return 1; }

    // Wire the worker callback now that we have the instance + extension data.
    g_inst = inst;
    if (d->extension_data)
        g_worker = (const LV2_Worker_Interface*)d->extension_data(LV2_WORKER__interface);
    if (!g_worker) { fprintf(stderr, "plugin has no worker interface\n"); return 1; }

    // Mirror Ardour: send time:Position only on transport *change* (cycle 0),
    // unless --pos-every-cycle is passed. --chunk N forces the state to be split
    // into N-byte payload chunks (to exercise the worker's reassembly path).
    // --loop P sends a luvie_loop atom on cycle 1 that switches the DSP into Loop
    // Mode with pattern P looping from bar 0 — the plugin-mode equivalent of hitting
    // the mode switch in the UI, which otherwise never reaches the DSP.
    // --unloop C [--resume B] then hands back to Song Mode at bar B on cycle C, the
    // way LoopModeController does once the loop phase lines up. The hand-off must be
    // silent: the run summary counts the Reset All Controllers (CC 121) and All Notes
    // Off (CC 123) messages that a transport relocate would produce, and for a clean
    // hand-off that count must stay at zero. --relocate C is the control case — a real
    // host relocate on cycle C, which *must* still produce those resets.
    // --song-loop S E arms the song editor's Start/End region [S,E), which the RT
    // engine wraps inside its own cycle. It rides on the cycle-1 atom, so combining it
    // with --loop also checks that Loop Mode does *not* wrap on the song region.
    // --worker-delay N models a real host's asynchronous worker thread (0 = inline).
    // --restate replays the whole project blob one cycle after the hand-off, which is
    // what the plugin UI really does: every mode settle fires onLoopStateChanged ->
    // sendState(), and the DSP then re-applies ports, instruments and the timeline.
    bool     posEveryCycle = false;
    uint32_t chunkBytes    = (uint32_t)json.size();  // default: single chunk
    int      loopPattern   = -1;
    int      unloopCycle   = -1;
    float    resumeBar     = 0.0f;
    int      relocateCycle = -1;
    int      cycles        = 0;      // 0 = default, see below
    bool     restate       = false;
    bool     songLoop      = false;
    float    songLoopStart = 0.0f, songLoopEnd = 0.0f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pos-every-cycle")) posEveryCycle = true;
        else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) chunkBytes = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--loop") && i + 1 < argc) loopPattern = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--unloop") && i + 1 < argc) unloopCycle = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--resume") && i + 1 < argc) resumeBar = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--relocate") && i + 1 < argc) relocateCycle = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--worker-delay") && i + 1 < argc) g_workerDelay = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--restate")) restate = true;
        else if (!strcmp(argv[i], "--cycles") && i + 1 < argc) cycles = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--song-loop") && i + 2 < argc) {
            songLoopStart = (float)atof(argv[++i]);
            songLoopEnd   = (float)atof(argv[++i]);
            songLoop      = true;
        }
    }
    if (chunkBytes == 0) chunkBytes = 1;

    // Port buffers: control_in (0) and every MIDI output (1 .. LUVIE_NUM_MIDI_OUTS).
    // control_in is sized to hold every chunk atom this harness emits in cycle 0 (a
    // real host would instead spread them across cycles), so even tiny --chunk sizes fit.
    const size_t numChunks  = (json.size() + chunkBytes - 1) / chunkBytes;
    const size_t perChunk   = sizeof(LuvieStateChunk) + chunkBytes + 64;  // + event/atom overhead
    std::vector<uint8_t> ctrlBuf(json.size() + numChunks * perChunk + 8192);
    std::vector<std::vector<uint8_t>> midiBuf(LUVIE_NUM_MIDI_OUTS,
                                              std::vector<uint8_t>(8192));
    d->connect_port(inst, 0, ctrlBuf.data());
    for (int o = 0; o < LUVIE_NUM_MIDI_OUTS; o++)
        d->connect_port(inst, (uint32_t)(PORT_OUT + o), midiBuf[o].data());

    if (d->activate) d->activate(inst);

    LV2_Atom_Forge forge;
    lv2_atom_forge_init(&forge, &map);
    LV2_URID uSeq   = map_uri(nullptr, LV2_ATOM__Sequence);
    LV2_URID uObj   = map_uri(nullptr, LV2_ATOM__Object);
    LV2_URID uPos   = map_uri(nullptr, LV2_TIME__Position);
    LV2_URID uFrame = map_uri(nullptr, LV2_TIME__frame);
    LV2_URID uSpeed = map_uri(nullptr, LV2_TIME__speed);
    LV2_URID uMidi  = map_uri(nullptr, LV2_MIDI__MidiEvent);
    LV2_URID uState = map_uri(nullptr, LUVIE_STATE_URI);
    LV2_URID uLoop  = map_uri(nullptr, LUVIE_LOOP_URI);

    int64_t frame = 0;
    int totalEmitted = 0;
    int totalResets  = 0;   // CC 121 / CC 123: the signature of a transport relocate
    int perPort[LUVIE_NUM_MIDI_OUTS] = {};
    // ~5.3 s at 256 frames / 48 kHz; --cycles runs longer, e.g. to watch a song loop
    // repeat enough times to measure whether its period drifts.
    cycles = cycles > 0 ? cycles : 1000;
    for (int c = 0; c < cycles; c++) {
        // Build control_in: a Sequence. On cycle 0 it carries the project state blob
        // (applied via the worker before playback) and the initial time:Position.
        LV2_Atom_Sequence* in = (LV2_Atom_Sequence*)ctrlBuf.data();
        in->atom.size = ctrlBuf.size() - sizeof(LV2_Atom);
        in->atom.type = uSeq;
        lv2_atom_forge_set_buffer(&forge, ctrlBuf.data(), ctrlBuf.size());
        LV2_Atom_Forge_Frame seqF;
        lv2_atom_forge_sequence_head(&forge, &seqF, 0);
        if (c == 0 || (restate && unloopCycle >= 0 && c == unloopCycle + 1)) {
            if (c) printf("cycle %d: re-sent full project state (as onLoopStateChanged does)\n", c);
            // Send the project as LuvieStateChunk header + payload atoms, split into
            // chunkBytes-sized slices (default one chunk).
            uint32_t total = (uint32_t)json.size();
            for (uint32_t off = 0; off < total; off += chunkBytes) {
                uint32_t cs = std::min(chunkBytes, total - off);
                LuvieStateChunk hdr{ 1, total, off, cs };
                lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_atom(&forge, (uint32_t)(sizeof(hdr) + cs), uState);
                lv2_atom_forge_write(&forge, &hdr, (uint32_t)sizeof(hdr));
                lv2_atom_forge_write(&forge, json.data() + off, cs);
            }
        }
        if (c == 1 && (loopPattern >= 0 || songLoop)) {
            // LuvieStateChunk header with msgId 0 marks a loop message, not JSON.
            const uint32_t inLoop = loopPattern >= 0 ? 1u : 0u;
            LuvieLoopState ls{ inLoop, inLoop, songLoop ? 1u : 0u,
                               songLoopStart, songLoopEnd, 0, 0.0f };
            LuvieLoopEntry le{ loopPattern, 0.0f, LUVIE_LOOP_ACTIVE | LUVIE_LOOP_MANUAL };
            forgeLoopAtom(&forge, uLoop, ls, inLoop ? &le : nullptr, inLoop);
            printf("cycle 1: sent luvie_loop (%s, pattern %d, songLoop=%d [%.2f,%.2f))\n",
                   inLoop ? "loop mode on" : "song mode", loopPattern,
                   (int)songLoop, songLoopStart, songLoopEnd);
        }
        if (c == unloopCycle) {
            // Loop -> Song hand-off: mode off, resume bar carried in the same atom so
            // the DSP moves its own musical position instead of the host relocating.
            LuvieLoopState ls{ 0, 0, songLoop ? 1u : 0u, songLoopStart, songLoopEnd,
                               1, resumeBar };
            forgeLoopAtom(&forge, uLoop, ls, nullptr, 0);
            printf("cycle %d: sent luvie_loop (song mode, hand-off to bar %.2f)\n",
                   c, resumeBar);
        }
        // A host relocate: the frame the host reports skips ahead, which the DSP sees
        // as a jump. Unlike the hand-off above, this is a *transport* break and must
        // still silence notes and reset controllers.
        if (c == relocateCycle) {
            frame += 64 * (int64_t)nframes;
            printf("cycle %d: host relocate to frame %ld\n", c, (long)frame);
        }
        if (posEveryCycle || c == 0 || c == relocateCycle) {
            lv2_atom_forge_frame_time(&forge, 0);
            LV2_Atom_Forge_Frame objF;
            lv2_atom_forge_object(&forge, &objF, 0, uPos);
            lv2_atom_forge_key(&forge, uFrame);
            lv2_atom_forge_long(&forge, frame);
            lv2_atom_forge_key(&forge, uSpeed);
            lv2_atom_forge_float(&forge, 1.0f);
            lv2_atom_forge_pop(&forge, &objF);
        }
        lv2_atom_forge_pop(&forge, &seqF);

        // Output ports: host sets capacity in atom.size before run().
        for (int o = 0; o < LUVIE_NUM_MIDI_OUTS; o++) {
            LV2_Atom_Sequence* mo = (LV2_Atom_Sequence*)midiBuf[o].data();
            mo->atom.size = midiBuf[o].size() - sizeof(LV2_Atom);
            mo->atom.type = uSeq;
        }

        g_cycle = c;
        drainWork(c);          // the host's worker thread catching up
        d->run(inst, nframes);

        // Dump each output separately, so it is visible which port a note went to.
        int posThisCycle = 0;
        for (int o = 0; o < LUVIE_NUM_MIDI_OUTS; o++) {
            LV2_Atom_Sequence* mo = (LV2_Atom_Sequence*)midiBuf[o].data();
            LV2_ATOM_SEQUENCE_FOREACH(mo, ev) {
                if (ev->body.type == uMidi) {
                    const uint8_t* m = (const uint8_t*)(ev + 1);
                    if (ev->body.size >= 2 && (m[0] & 0xF0) == 0xB0 &&
                        (m[1] == 121 || m[1] == 123))
                        totalResets++;
                    printf("cycle %d frame %ld @%ld out%d: ",
                           c, (long)frame, (long)ev->time.frames, o + 1);
                    for (uint32_t i = 0; i < ev->body.size; i++) printf("%02X ", m[i]);
                    printf("\n");
                    totalEmitted++;
                    perPort[o]++;
                } else if (ev->body.type == uObj) {
                    posThisCycle++;
                }
            }
        }
        if (c == 0) printf("cycle 0: %d non-MIDI (Position/State) atoms on the ports\n", posThisCycle);
        frame += nframes;
    }

    printf("TOTAL MIDI events emitted: %d\n", totalEmitted);
    printf("Reset All Controllers / All Notes Off messages: %d\n", totalResets);
    for (int o = 0; o < LUVIE_NUM_MIDI_OUTS; o++)
        if (perPort[o]) printf("  midi_out %d: %d events\n", o + 1, perPort[o]);
    if (d->cleanup) d->cleanup(inst);
    dlclose(h);
    return 0;
}
