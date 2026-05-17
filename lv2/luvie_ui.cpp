#include "lv2_external_ui.h"
#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <cstdio>
#include <unistd.h>

#include <FL/Fl.H>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

/* Force FLTK to use X11 (not Wayland) when running as an LV2 plugin inside
   a Qt/Wayland host like Carla. FLTK looks up this symbol via dlsym() at
   display-init time and skips Wayland if it is true. */
extern "C" FL_EXPORT bool fl_disable_wayland = true;

#include "../src/appWindow.hpp"
#include "../src/transport.hpp"
#include "../src/simpleTransport.hpp"
#include "../src/jackTransport.hpp"
#include "../src/observableSong.hpp"
#include "../src/observablePattern.hpp"
#include "../src/outputsOverlay.hpp"
#include "../src/patternPanel.hpp"
#include "../src/chords.hpp"
#include "../src/luvieApp.hpp"
#include "../src/timelineIO.hpp"
#include "timeline_serial.h"

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"

/* File used to pass state between DSP restore and UI open, and between UI sessions */
static std::string g_stateFilePath;

/* -----------------------------------------------------------------------
   Forward declarations
   ----------------------------------------------------------------------- */

struct LuvieUI;
static void sendTimeline(LuvieUI* ui);
static std::string serializeStateToString(LuvieUI* ui);
static void sendFullState(LuvieUI* ui);
static void deserializeFullState(LuvieUI* ui, const uint8_t* data, uint32_t size);

/* -----------------------------------------------------------------------
   LuvieUI — the ExternalUI instance.
   ----------------------------------------------------------------------- */

struct LuvieUI {
    /* Must be first — the host casts our handle to this type. */
    LV2_External_UI_Widget widget;

    LV2UI_Controller        controller  = nullptr;
    LV2UI_Write_Function    writeFunc   = nullptr;
    LV2_External_UI_Host*   extHost     = nullptr;
    LV2_URID_Map*           map         = nullptr;
    LV2_URID                atom_eventTransfer  = 0;
    LV2_URID                luvie_timeline_urid = 0;
    LV2_URID                luvie_state_urid    = 0;
    LV2_URID                atom_Object         = 0;
    LV2_URID                atom_Blank          = 0;
    LV2_URID                time_Position       = 0;
    LV2_URID                time_bar            = 0;
    LV2_URID                time_barBeat        = 0;
    LV2_URID                time_beatsPerBar    = 0;
    LV2_URID                time_beatsPerMinute = 0;
    LV2_URID                time_speed          = 0;

    /* Owned heap objects */
    AppWindow*          window         = nullptr;
    ObservableSong*     song           = nullptr;
    ObservablePattern*  pattern        = nullptr;
    SimpleTransport*    simpleTransport= nullptr;
    JackTransport*      jackTransport  = nullptr;

    /* UI layout (owns FLTK widgets via window) */
    LuvieApp            app;

    /* Set during state restore to suppress re-sending while rebuilding */
    bool restoringState = false;

    ~LuvieUI() {
        /* app destructor removes timeline observers */
        /* Widgets are owned by window (added via add()), deleted with window */
        delete window;
        delete jackTransport;
        delete simpleTransport;
        delete pattern;
        delete song;
    }

    /* Try once to open JACK (control-only, no MIDI). Enables buttons on success,
       leaves them disabled if JACK is not available. */
    void tryConnectJack() {
        auto* jt = new JackTransport();
        if (jt->open("luvie_lv2", /*enableMidi=*/false)) {
            jackTransport = jt;
            jackTransport->setTimeline(song);
            jackTransport->onTransportEvent = [this]() {
                Fl::awake([](void* data) {
                    static_cast<LuvieUI*>(data)->app.bottomPane->syncPlayState();
                }, this);
            };
            if (app.bottomPane) app.bottomPane->setControlTransport(jackTransport);
        } else {
            delete jt;
        }
    }
};

/* -----------------------------------------------------------------------
   Timeline serialization + send
   ----------------------------------------------------------------------- */

static void sendTimeline(LuvieUI* ui)
{
    if (!ui->writeFunc || !ui->song || !ui->app.patternPanel)
        return;

    const Timeline& tl        = ui->song->get();
    const int rootPitch        = ui->app.patternPanel->rootPitch();
    const int chordType        = ui->app.patternPanel->chordType();
    const ChordDef& chord      = chordDefs[chordType];
    const int chordSize        = chord.size;
    const int rootSemitone     = (rootPitch + 9) % 12;
    const int rootMidi0        = 12 + rootSemitone;

    const uint32_t numTimeSigs = (uint32_t)tl.timeSigs.size();
    const uint32_t numBpms     = (uint32_t)tl.bpms.size();
    const uint32_t numPatterns = (uint32_t)tl.patterns.size();
    const uint32_t numTracks   = (uint32_t)tl.tracks.size();

    /* Pre-count enabled notes per pattern */
    std::vector<uint32_t> noteCounts(numPatterns, 0);
    for (uint32_t p = 0; p < numPatterns; p++)
        for (const auto& n : tl.patterns[p].notes)
            if (!n.disabled) noteCounts[p]++;

    /* Calculate total binary size */
    size_t dataSize = sizeof(TimelineHeader)
                    + numTimeSigs * sizeof(DspTimeSig)
                    + numBpms    * sizeof(DspBpmMarker);
    for (uint32_t p = 0; p < numPatterns; p++)
        dataSize += sizeof(SerialPatternHeader) + noteCounts[p] * sizeof(DspNote);
    for (const auto& track : tl.tracks)
        dataSize += sizeof(SerialTrackHeader) + track.patterns.size() * sizeof(DspPatternInstance);

    /* Build buffer: LV2_Atom header followed by binary data */
    const size_t totalSize = sizeof(LV2_Atom) + dataSize;
    std::vector<uint8_t> buf(totalSize);

    LV2_Atom* atom = reinterpret_cast<LV2_Atom*>(buf.data());
    atom->type = ui->luvie_timeline_urid;
    atom->size = (uint32_t)dataSize;

    uint8_t* p = buf.data() + sizeof(LV2_Atom);

    /* TimelineHeader */
    TimelineHeader hdr = { numTimeSigs, numBpms, numPatterns, numTracks };
    memcpy(p, &hdr, sizeof(hdr)); p += sizeof(hdr);

    /* DspTimeSig[] */
    for (const auto& ts : tl.timeSigs) {
        DspTimeSig dts = { ts.bar, ts.top, ts.bottom };
        memcpy(p, &dts, sizeof(dts)); p += sizeof(dts);
    }

    /* DspBpmMarker[] */
    for (const auto& bm : tl.bpms) {
        DspBpmMarker dbm = { bm.bar, bm.bpm };
        memcpy(p, &dbm, sizeof(dbm)); p += sizeof(dbm);
    }

    /* Patterns */
    for (uint32_t pi = 0; pi < numPatterns; pi++) {
        const auto& pat = tl.patterns[pi];
        SerialPatternHeader sph = { pat.id, pat.lengthBeats, noteCounts[pi] };
        memcpy(p, &sph, sizeof(sph)); p += sizeof(sph);

        for (const auto& n : pat.notes) {
            if (n.disabled) continue;
            const int ni     = (int)n.pitch;
            const int degree = ni % chordSize;
            const int octave = ni / chordSize;
            const int midi   = std::clamp(
                rootMidi0 + chord.intervals[degree] + octave * 12, 0, 127);
            const float vel  = n.velocity > 0.0f ? n.velocity : 0.8f;
            DspNote dn = { n.beat, n.length,
                           (uint8_t)midi,
                           (uint8_t)std::clamp((int)(vel * 127.0f + 0.5f), 1, 127),
                           {0, 0} };
            memcpy(p, &dn, sizeof(dn)); p += sizeof(dn);
        }
    }

    /* Build instrument name → 0-indexed MIDI channel map */
    std::map<std::string, uint8_t> instrChannelMap;
    if (ui->app.outputsOverlay) {
        for (const auto& ci : ui->app.outputsOverlay->getInstruments())
            instrChannelMap[ci.name] = (uint8_t)std::clamp(ci.midiChannel - 1, 0, 15);
    }

    /* Tracks */
    for (const auto& track : tl.tracks) {
        uint8_t channel = 0;
        for (const auto& pat : tl.patterns) {
            if (pat.id == track.patternId && !pat.outputInstrumentName.empty()) {
                auto it = instrChannelMap.find(pat.outputInstrumentName);
                if (it != instrChannelMap.end()) channel = it->second;
                break;
            }
        }
        SerialTrackHeader sth = { track.id, channel, {0,0,0},
                                  (uint32_t)track.patterns.size() };
        memcpy(p, &sth, sizeof(sth)); p += sizeof(sth);

        for (const auto& inst : track.patterns) {
            DspPatternInstance dpi = {
                inst.id, inst.patternId, inst.startBar,
                inst.length, inst.startOffset
            };
            memcpy(p, &dpi, sizeof(dpi)); p += sizeof(dpi);
        }
    }

    ui->writeFunc(ui->controller,
                  0 /* PORT_CONTROL_IN */,
                  (uint32_t)totalSize,
                  ui->atom_eventTransfer,
                  buf.data());
}

/* -----------------------------------------------------------------------
   Full JSON state serialization / deserialization
   ----------------------------------------------------------------------- */

static std::string serializeStateToString(LuvieUI* ui)
{
    if (!ui->song || !ui->app.patternPanel)
        return {};
    AppState state;
    state.timeline  = ui->song->get();
    state.rootPitch = ui->app.patternPanel->rootPitch();
    state.chordType = ui->app.patternPanel->chordType();
    state.sharp     = ui->app.patternPanel->isSharp();
    if (ui->app.outputsOverlay)
        for (const auto& ci : ui->app.outputsOverlay->getInstruments())
            state.jackInstruments.push_back({ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                                             ci.isDrum, ci.fallbackNoteNames,
                                             ci.programNumber, ci.bankMsb, ci.bankLsb,
                                             ci.gm1Instrument});
    return appStateToJsonString(state);
}

static void sendFullState(LuvieUI* ui)
{
    if (!ui->writeFunc) return;

    std::string jsonStr = serializeStateToString(ui);
    if (jsonStr.empty()) return;

    /* Include null terminator so the DSP can treat it as a C string */
    uint32_t jsonSize = (uint32_t)jsonStr.size() + 1;

    std::vector<uint8_t> atomBuf(sizeof(LV2_Atom) + jsonSize);
    LV2_Atom* atom = reinterpret_cast<LV2_Atom*>(atomBuf.data());
    atom->type = ui->luvie_state_urid;
    atom->size = jsonSize;
    memcpy(atomBuf.data() + sizeof(LV2_Atom), jsonStr.c_str(), jsonSize);

    ui->writeFunc(ui->controller,
                  0 /* PORT_CONTROL_IN */,
                  (uint32_t)atomBuf.size(),
                  ui->atom_eventTransfer,
                  atomBuf.data());
}

static void deserializeFullState(LuvieUI* ui, const uint8_t* data, uint32_t size)
{
    if (!ui->song || !ui->app.patternPanel || size == 0) return;
    AppState state;
    if (!appStateFromJsonString(std::string(reinterpret_cast<const char*>(data), size), state)) {
        fprintf(stderr, "[luvie_ui] deserializeFullState: JSON parse failed\n");
        return;
    }
    ui->restoringState = true;
    ui->app.patternPanel->setParams(state.rootPitch, state.chordType, state.sharp);
    ui->song->loadTimeline(state.timeline);
    if (ui->app.outputsOverlay) {
        std::vector<OutputsOverlay::InstrumentInfo> instrs;
        for (const auto& c : state.jackInstruments)
            instrs.push_back({0, c.name, c.portName, c.midiChannel, c.drumMap,
                              c.isDrum, c.fallbackNoteNames, c.programNumber, c.bankMsb, c.bankLsb,
                              c.gm1Instrument});
        ui->app.outputsOverlay->setInstruments(instrs);
        ui->app.pushInstruments();
    }
    ui->restoringState = false;
    sendTimeline(ui);
    sendFullState(ui);
}

/* -----------------------------------------------------------------------
   ExternalUI function-pointer callbacks
   ----------------------------------------------------------------------- */

static void ui_run(LV2_External_UI_Widget* w)
{
    (void)w;
    Fl::check();
}

static void ui_show(LV2_External_UI_Widget* w)
{
    LuvieUI* ui = reinterpret_cast<LuvieUI*>(w);
    if (ui->window)
        ui->window->show();
}

static void ui_hide(LV2_External_UI_Widget* w)
{
    LuvieUI* ui = reinterpret_cast<LuvieUI*>(w);
    if (ui->window)
        ui->window->hide();
}

/* Called when the user closes the window via the WM button. */
static void window_close_cb(Fl_Widget* /*w*/, void* data)
{
    LuvieUI* ui = static_cast<LuvieUI*>(data);
    ui->window->hide();
    if (ui->extHost && ui->extHost->ui_closed)
        ui->extHost->ui_closed(ui->controller);
}

/* -----------------------------------------------------------------------
   LV2 UI callbacks
   ----------------------------------------------------------------------- */

static LV2UI_Handle instantiate(
    const LV2UI_Descriptor*   /*descriptor*/,
    const char*               /*plugin_uri*/,
    const char*               /*bundle_path*/,
    LV2UI_Write_Function      write_function,
    LV2UI_Controller          controller,
    LV2UI_Widget*             widget,
    const LV2_Feature* const* features)
{
    LuvieUI* ui = new LuvieUI;
    ui->widget.run  = ui_run;
    ui->widget.show = ui_show;
    ui->widget.hide = ui_hide;
    ui->controller  = controller;
    ui->writeFunc   = write_function;

    /* Scan features */
    for (int i = 0; features[i]; i++) {
        if (std::strcmp(features[i]->URI, LV2_EXTERNAL_UI_HOST_URI) == 0)
            ui->extHost = static_cast<LV2_External_UI_Host*>(features[i]->data);
        else if (std::strcmp(features[i]->URI, LV2_EXTERNAL_UI_DEPRECATED_URI) == 0
                 && !ui->extHost)
            ui->extHost = static_cast<LV2_External_UI_Host*>(features[i]->data);
        else if (std::strcmp(features[i]->URI, LV2_URID__map) == 0)
            ui->map = static_cast<LV2_URID_Map*>(features[i]->data);
    }

    /* Build the per-process state file path once (PID is stable for the session) */
    if (g_stateFilePath.empty()) {
        char buf[256];
        snprintf(buf, sizeof(buf), "/tmp/luvie_state_%d.json", (int)getpid());
        g_stateFilePath = buf;
    }

    if (ui->map) {
        auto m = [&](const char* uri) { return ui->map->map(ui->map->handle, uri); };
        ui->atom_eventTransfer  = m(LV2_ATOM__eventTransfer);
        ui->luvie_timeline_urid = m(LUVIE_TIMELINE_URI);
        ui->luvie_state_urid    = m(LUVIE_STATE_URI);
        ui->atom_Object         = m(LV2_ATOM__Object);
        ui->atom_Blank          = m(LV2_ATOM__Blank);
        ui->time_Position       = m(LV2_TIME__Position);
        ui->time_bar            = m(LV2_TIME__bar);
        ui->time_barBeat        = m(LV2_TIME__barBeat);
        ui->time_beatsPerBar    = m(LV2_TIME__beatsPerBar);
        ui->time_beatsPerMinute = m(LV2_TIME__beatsPerMinute);
        ui->time_speed          = m(LV2_TIME__speed);
    }

    /* ---- Window ---- */
    const char* title = (ui->extHost && ui->extHost->plugin_human_id)
                        ? ui->extHost->plugin_human_id : "Luvie";
    ui->window = new AppWindow(LuvieApp::winW, LuvieApp::defaultWinH());
    ui->window->label(title);
    ui->window->color(bgColor);
    ui->window->callback(window_close_cb, ui);
    ui->window->end();

    /* ---- Timeline + transport ---- */
    ui->song    = new ObservableSong(120.0f, 4, 4);
    ui->pattern = new ObservablePattern(ui->song);
    {
        int patId = ui->pattern->createPattern(LuvieApp::numPatternBeats);
        ui->song->addTrack("Pattern 1", patId);
    }
    ui->simpleTransport = new SimpleTransport;
    ui->simpleTransport->setTimeline(ui->song);

    /* ---- LuvieApp callbacks ---- */
    ui->app.disableTransportButtons = true;

    ui->app.onExtraTimelineChange = [ui]() {
        if (!ui->restoringState) {
            sendTimeline(ui);
            sendFullState(ui);
        }
    };

    ui->app.onExtraSeek = [ui]() {
        if (ui->jackTransport)
            ui->jackTransport->seek(ui->simpleTransport->position());
    };

    ui->app.onExtraParamsChanged = [ui]() {
        if (!ui->restoringState) {
            sendTimeline(ui);
            sendFullState(ui);
        }
    };

    /* ---- Build all shared UI ---- */
    ui->app.build(ui->window, ui->song, ui->pattern, ui->simpleTransport);
    ui->app.disableSaveMenu(/*save=*/true, /*saveAs=*/true);

    /* ---- Wire DSP sends when instruments change ---- */
    ui->app.onInstrumentsChanged = [ui]() {
        if (!ui->restoringState) {
            sendTimeline(ui);
            sendFullState(ui);
        }
    };

    /* ---- Restore from file if available ---- */
    if (!g_stateFilePath.empty()) {
        FILE* f = fopen(g_stateFilePath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                std::vector<uint8_t> buf((size_t)sz);
                if (fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) buf.clear();
                deserializeFullState(ui, buf.data(), (uint32_t)sz);
            }
            fclose(f);
        }
    }

    sendTimeline(ui);

    /* ---- Try to connect to JACK for transport control ---- */
    ui->tryConnectJack();

    /* Return the widget pointer as the LV2UI handle */
    *widget = &ui->widget;
    return reinterpret_cast<LV2UI_Handle>(ui);
}

static void cleanup(LV2UI_Handle handle)
{
    LuvieUI* ui = reinterpret_cast<LuvieUI*>(handle);

    /* Save current state to file so the next instantiate can restore it */
    if (!g_stateFilePath.empty()) {
        std::string json = serializeStateToString(ui);
        if (!json.empty()) {
            FILE* f = fopen(g_stateFilePath.c_str(), "wb");
            if (f) {
                fwrite(json.c_str(), 1, json.size(), f);
                fclose(f);
            }
        }
    }

    delete ui;
}

static void port_event(LV2UI_Handle handle, uint32_t port_index,
                       uint32_t /*buffer_size*/, uint32_t /*format*/,
                       const void* buffer)
{
    LuvieUI* ui = reinterpret_cast<LuvieUI*>(handle);
    if (port_index != 2)
        return;

    const LV2_Atom* atom = static_cast<const LV2_Atom*>(buffer);

    /* Handle state sent back from DSP (belt-and-suspenders: file is primary path) */
    if (atom->type == ui->luvie_state_urid) {
        const uint8_t* body = reinterpret_cast<const uint8_t*>(atom) + sizeof(LV2_Atom);
        deserializeFullState(ui, body, atom->size);
        return;
    }

    if ((atom->type != ui->atom_Object && atom->type != ui->atom_Blank) ||
        !ui->simpleTransport)
        return;

    const LV2_Atom_Object* obj = reinterpret_cast<const LV2_Atom_Object*>(atom);
    if (obj->body.otype != ui->time_Position)
        return;

    const LV2_Atom_Long*  aBar   = nullptr;
    const LV2_Atom_Float* aBeat  = nullptr;
    const LV2_Atom_Float* aBpb   = nullptr;
    const LV2_Atom_Float* aSpeed = nullptr;
    lv2_atom_object_get(obj,
        ui->time_bar,         &aBar,
        ui->time_barBeat,     &aBeat,
        ui->time_beatsPerBar, &aBpb,
        ui->time_speed,       &aSpeed,
        0);

    if (!aBar && !aBeat)
        return;

    float bar     = aBar  ? (float)aBar->body  : 0.0f;
    float barBeat = aBeat ? aBeat->body         : 0.0f;
    float bpb     = aBpb  ? aBpb->body          : 4.0f;
    bool  playing = aSpeed && aSpeed->body != 0.0f;

    /* Convert bar + barBeat to an absolute bar position (fractional bars) */
    float bars = bar + barBeat / bpb;
    ui->simpleTransport->syncFromHost(bars, playing);
    if (ui->app.bottomPane) ui->app.bottomPane->syncPlayState();
}

static const void* extension_data(const char* uri)
{
    (void)uri;
    return nullptr;
}

static const LV2UI_Descriptor descriptor = {
    "https://github.com/benbriedis/luvie/luvie_ui",
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : nullptr;
}
