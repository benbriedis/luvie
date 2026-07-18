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

// Minimal worker: a real host runs work() on a non-RT thread; for the test we run
// it synchronously inside schedule_work (the DSP only calls this from run()).
static const LV2_Worker_Interface* g_worker = nullptr;
static LV2_Handle                  g_inst   = nullptr;

static LV2_Worker_Status test_respond(LV2_Worker_Respond_Handle, uint32_t, const void*) {
    return LV2_WORKER_SUCCESS;
}
static LV2_Worker_Status test_schedule(LV2_Worker_Schedule_Handle, uint32_t size, const void* data) {
    if (g_worker && g_worker->work)
        g_worker->work(g_inst, test_respond, nullptr, size, data);
    return LV2_WORKER_SUCCESS;
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
    bool     posEveryCycle = false;
    uint32_t chunkBytes    = (uint32_t)json.size();  // default: single chunk
    int      loopPattern   = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pos-every-cycle")) posEveryCycle = true;
        else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) chunkBytes = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--loop") && i + 1 < argc) loopPattern = atoi(argv[++i]);
    }
    if (chunkBytes == 0) chunkBytes = 1;

    // Port buffers: control_in (0) and the single merged output (1). control_in is
    // sized to hold every chunk atom this harness emits in cycle 0 (a real host
    // would instead spread them across cycles), so even tiny --chunk sizes fit.
    const size_t numChunks  = (json.size() + chunkBytes - 1) / chunkBytes;
    const size_t perChunk   = sizeof(LuvieStateChunk) + chunkBytes + 64;  // + event/atom overhead
    std::vector<uint8_t> ctrlBuf(json.size() + numChunks * perChunk + 8192), midiBuf(8192);
    d->connect_port(inst, 0, ctrlBuf.data());
    d->connect_port(inst, 1, midiBuf.data());

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
    const int cycles = 1000;  // ~5.3 s at 256 frames / 48 kHz
    for (int c = 0; c < cycles; c++) {
        // Build control_in: a Sequence. On cycle 0 it carries the project state blob
        // (applied via the worker before playback) and the initial time:Position.
        LV2_Atom_Sequence* in = (LV2_Atom_Sequence*)ctrlBuf.data();
        in->atom.size = ctrlBuf.size() - sizeof(LV2_Atom);
        in->atom.type = uSeq;
        lv2_atom_forge_set_buffer(&forge, ctrlBuf.data(), ctrlBuf.size());
        LV2_Atom_Forge_Frame seqF;
        lv2_atom_forge_sequence_head(&forge, &seqF, 0);
        if (c == 0) {
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
        if (c == 1 && loopPattern >= 0) {
            // LuvieStateChunk header with msgId 0 marks a loop message, not JSON.
            LuvieStateChunk mark{ 0, (uint32_t)(sizeof(LuvieLoopState) + sizeof(LuvieLoopEntry)), 0, 0 };
            LuvieLoopState  ls{ 1, 1 };
            LuvieLoopEntry  le{ loopPattern, 0.0f, LUVIE_LOOP_ACTIVE | LUVIE_LOOP_MANUAL };
            lv2_atom_forge_frame_time(&forge, 0);
            lv2_atom_forge_atom(&forge, (uint32_t)(sizeof(mark) + sizeof(ls) + sizeof(le)), uLoop);
            lv2_atom_forge_write(&forge, &mark, (uint32_t)sizeof(mark));
            lv2_atom_forge_write(&forge, &ls,   (uint32_t)sizeof(ls));
            lv2_atom_forge_write(&forge, &le,   (uint32_t)sizeof(le));
            printf("cycle 1: sent luvie_loop (loop mode on, pattern %d)\n", loopPattern);
        }
        if (posEveryCycle || c == 0) {
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

        // Output port: host sets capacity in atom.size before run().
        LV2_Atom_Sequence* mo = (LV2_Atom_Sequence*)midiBuf.data();
        mo->atom.size = midiBuf.size() - sizeof(LV2_Atom);
        mo->atom.type = uSeq;

        d->run(inst, nframes);

        // Dump the merged output: MIDI events (and note Position objects exist too).
        int posThisCycle = 0;
        LV2_ATOM_SEQUENCE_FOREACH(mo, ev) {
            if (ev->body.type == uMidi) {
                const uint8_t* m = (const uint8_t*)(ev + 1);
                printf("cycle %d frame %ld @%ld: ", c, (long)frame, (long)ev->time.frames);
                for (uint32_t i = 0; i < ev->body.size; i++) printf("%02X ", m[i]);
                printf("\n");
                totalEmitted++;
            } else if (ev->body.type == uObj) {
                posThisCycle++;
            }
        }
        if (c == 0) printf("cycle 0: %d non-MIDI (Position/State) atoms on the port\n", posThisCycle);
        frame += nframes;
    }

    printf("TOTAL MIDI events emitted: %d\n", totalEmitted);
    if (d->cleanup) d->cleanup(inst);
    dlclose(h);
    return 0;
}
