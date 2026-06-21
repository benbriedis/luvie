// Minimal LV2 host harness: dlopen luvie_dsp.so, feed it a state file + a
// time:Position stream, and dump whatever it forges onto midi_out. Used to
// isolate "DSP isn't emitting" from "Ardour isn't routing".
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <chrono>

#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <lv2/midi/midi.h>

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

int main(int argc, char** argv) {
    const char* so    = argc > 1 ? argv[1] : "build/luvie.lv2/luvie_dsp.so";
    const char* state = argc > 2 ? argv[2] : "/tmp/luvie_state_240663.json";
    const double sr   = 48000.0;
    const uint32_t nframes = 256;

    // Copy the state file to the path the DSP will poll (our own pid).
    char statePath[256];
    snprintf(statePath, sizeof(statePath), "/tmp/luvie_state_%d.json", (int)getpid());
    {
        FILE* in = fopen(state, "rb"); if (!in) { perror("state"); return 1; }
        FILE* out = fopen(statePath, "wb");
        char b[4096]; size_t n;
        while ((n = fread(b,1,sizeof b,in))>0) fwrite(b,1,n,out);
        fclose(in); fclose(out);
    }
    printf("wrote state to %s\n", statePath);

    void* h = dlopen(so, RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    auto desc_fn = (const LV2_Descriptor*(*)(uint32_t))dlsym(h, "lv2_descriptor");
    if (!desc_fn) { fprintf(stderr, "no lv2_descriptor\n"); return 1; }
    const LV2_Descriptor* d = desc_fn(0);
    printf("plugin URI: %s\n", d->URI);

    LV2_URID_Map map{ nullptr, map_uri };
    LV2_Feature mapF{ LV2_URID__map, &map };
    const LV2_Feature* features[] = { &mapF, nullptr };

    LV2_Handle inst = d->instantiate(d, sr, "build/luvie.lv2/", features);
    if (!inst) { fprintf(stderr, "instantiate failed\n"); return 1; }

    // Port buffers: control_in (0) and the single merged output (1).
    std::vector<uint8_t> ctrlBuf(8192), midiBuf(8192);
    d->connect_port(inst, 0, ctrlBuf.data());
    d->connect_port(inst, 1, midiBuf.data());

    if (d->activate) d->activate(inst);

    // Give the poll thread time to read + apply the state file.
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    LV2_Atom_Forge forge;
    lv2_atom_forge_init(&forge, &map);
    LV2_URID uSeq   = map_uri(nullptr, LV2_ATOM__Sequence);
    LV2_URID uObj   = map_uri(nullptr, LV2_ATOM__Object);
    LV2_URID uPos   = map_uri(nullptr, LV2_TIME__Position);
    LV2_URID uFrame = map_uri(nullptr, LV2_TIME__frame);
    LV2_URID uSpeed = map_uri(nullptr, LV2_TIME__speed);
    LV2_URID uMidi  = map_uri(nullptr, LV2_MIDI__MidiEvent);

    // Mirror Ardour: send time:Position only on transport *change* (cycle 0),
    // unless --pos-every-cycle is passed.
    bool posEveryCycle = false;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--pos-every-cycle")) posEveryCycle = true;

    int64_t frame = 0;
    int totalEmitted = 0;
    const int cycles = 1000;  // ~5.3 s at 256 frames / 48 kHz
    for (int c = 0; c < cycles; c++) {
        // Build control_in: a Sequence, optionally with a time:Position(frame, speed=1).
        LV2_Atom_Sequence* in = (LV2_Atom_Sequence*)ctrlBuf.data();
        in->atom.size = ctrlBuf.size() - sizeof(LV2_Atom);
        in->atom.type = uSeq;
        lv2_atom_forge_set_buffer(&forge, ctrlBuf.data(), ctrlBuf.size());
        LV2_Atom_Forge_Frame seqF;
        lv2_atom_forge_sequence_head(&forge, &seqF, 0);
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
