#include "lv2_external_ui.h"
#include "luvie_dsp.h"
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

// luvie_core's include dirs (src/, src/modern, src/transports) propagate here.
#include "appWindow.hpp"
#include "transport.hpp"
#include "simpleTransport.hpp"
#include "jackTransport.hpp"
#include "observableSong.hpp"
#include "observablePattern.hpp"
#include "observableInstrument.hpp"
#include "outputsOverlay.hpp"
#include "patternPanel.hpp"
#include "luvieApp.hpp"
#include "timelineIO.hpp"

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"

/* File used to pass state between DSP restore and UI open, and between UI sessions */
static std::string g_stateFilePath;

/* -----------------------------------------------------------------------
   Forward declarations
   ----------------------------------------------------------------------- */

struct LuvieUI;
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
    AppWindow*            window         = nullptr;
    ObservableSong*       song           = nullptr;
    ObservablePattern*    pattern        = nullptr;
    ObservableInstrument* instruments    = nullptr;
    SimpleTransport*      simpleTransport= nullptr;
    JackTransport*        jackTransport  = nullptr;

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
        delete instruments;
        delete song;
    }

    /* Try once to open JACK with MIDI enabled. Registers all current output ports on
       success and enables transport-sync buttons. */
    void tryConnectJack() {
        auto* jt = new JackTransport();
        if (jt->open("luvie_lv2", /*enableMidi=*/true)) {
            jackTransport = jt;
            jackTransport->setTimeline(song);
            jackTransport->onTransportEvent = [this]() {
                Fl::awake([](void* data) {
                    static_cast<LuvieUI*>(data)->app.bottomPane->syncPlayState();
                }, this);
            };
            if (app.bottomPane) app.bottomPane->setControlTransport(jackTransport);
            if (app.outputsOverlay) {
                for (const auto& name : app.outputsOverlay->getOutputs())
                    jackTransport->addMidiPort(name);
                std::vector<JackTransport::InstrumentRouting> routings;
                for (const auto& ci : app.outputsOverlay->getInstruments())
                    routings.push_back({ci.id, ci.portName, ci.midiChannel,
                                        ci.programNumber, ci.bankMsb, ci.bankLsb});
                jackTransport->setInstruments(routings);
            }
        } else {
            delete jt;
        }
    }
};

/* -----------------------------------------------------------------------
   Timeline serialization + send
   ----------------------------------------------------------------------- */


/* -----------------------------------------------------------------------
   Full JSON state serialization / deserialization
   ----------------------------------------------------------------------- */

/* Read the Outputs overlay (ports + backends + default type + instruments)
   into `state`. Shared by LV2 state save and the File/Export menu. */
static void collectOverlayOutputs(LuvieUI* ui, AppState& state)
{
    auto* overlay = ui->app.outputsOverlay;
    if (!overlay) return;
    state.defaultPortBackend = overlay->getDefaultBackend();
    state.jackOutputs        = overlay->getOutputsFull();  // names + backends
    state.jackInstruments.clear();
    for (const auto& ci : overlay->getInstruments())
        state.jackInstruments.push_back({ci.id, ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                                         ci.isDrum, ci.fallbackNoteNames,
                                         ci.programNumber, ci.bankMsb, ci.bankLsb,
                                         ci.gm1Instrument});
}

/* Push a loaded `state` into the Outputs overlay and re-register JACK ports to
   match. Shared by LV2 state restore and the File/Import menu. */
static void applyOverlayOutputs(LuvieUI* ui, const AppState& state)
{
    auto* overlay = ui->app.outputsOverlay;
    if (!overlay) return;
    overlay->setDefaultBackend(state.defaultPortBackend);
    if (ui->jackTransport)
        for (const auto& name : overlay->getOutputs())
            ui->jackTransport->removeMidiPort(name);
    if (!state.jackOutputs.empty())
        overlay->setOutputs(state.jackOutputs);  // JackOutput overload keeps backends
    if (ui->jackTransport)
        for (const auto& name : overlay->getOutputs())
            ui->jackTransport->addMidiPort(name);
    std::vector<OutputsOverlay::InstrumentInfo> instrs;
    for (const auto& c : state.jackInstruments)
        instrs.push_back({c.id, c.name, c.portName, c.midiChannel, c.drumMap,
                          c.isDrum, c.fallbackNoteNames, c.programNumber, c.bankMsb, c.bankLsb,
                          c.gm1Instrument});
    overlay->setInstruments(instrs);
    ui->app.pushInstruments();
}

static std::string serializeStateToString(LuvieUI* ui)
{
    if (!ui->song || !ui->app.patternPanel)
        return {};
    AppState state;
    state.timeline  = ui->song->get();
    state.rootPitch = ui->app.patternPanel->rootPitch();
    state.chordType = ui->app.patternPanel->chordType();
    state.sharp     = ui->app.patternPanel->isSharp();
    collectOverlayOutputs(ui, state);
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
    applyOverlayOutputs(ui, state);
    ui->restoringState = false;
    /* No sendFullState() here: the DSP already holds this exact state (it just
       restored it, or we share its persistent instance). Echoing it back would
       trip the DSP's stateChanged flag and make the host report a spurious
       unsaved-change right after load. */
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
    ui->song        = new ObservableSong(120.0f, 4, 4);
    ui->pattern     = new ObservablePattern(ui->song);
    ui->instruments = new ObservableInstrument(ui->song);
    /* build() seeds default instruments + their tracks for an empty session. */
    ui->simpleTransport = new SimpleTransport;
    ui->simpleTransport->setTimeline(ui->song);

    /* ---- LuvieApp callbacks ---- */
    ui->app.disableTransportButtons = true;
    ui->app.pluginMode              = true;

    ui->app.onExtraTimelineChange = [ui]() {
        if (!ui->restoringState) sendFullState(ui);
    };

    ui->app.onExtraSeek = [ui]() {
        if (ui->jackTransport)
            ui->jackTransport->seek(ui->simpleTransport->position());
    };

    ui->app.onExtraParamsChanged = [ui]() {
        if (!ui->restoringState) sendFullState(ui);
    };

    /* ---- Build all shared UI ---- */
    ui->app.build(ui->window, ui->song, ui->pattern, ui->instruments, ui->simpleTransport);
    ui->app.disableSaveMenu(/*save=*/true, /*saveAs=*/true);

    /* ---- Wire port management (same as standalone) ---- */
    if (auto* overlay = ui->app.outputsOverlay) {
        /* No new-project dialog in the plugin; default MIDI output is Jack. */
        overlay->setDefaultBackend(MidiBackend::Jack);
        overlay->onPortAdded = [ui](const std::string& name) {
            if (ui->jackTransport) ui->jackTransport->addMidiPort(name);
        };
        overlay->onPortRemoved = [ui](const std::string& name) {
            if (ui->jackTransport) ui->jackTransport->removeMidiPort(name);
        };
        overlay->onPortRenamed = [ui](const std::string& oldName, const std::string& newName) {
            if (ui->jackTransport) ui->jackTransport->renameMidiPort(oldName, newName);
        };
    }

    /* ---- Wire DSP sends and JACK routings when instruments change ---- */
    ui->app.onInstrumentsChanged = [ui]() {
        if (ui->jackTransport && ui->app.outputsOverlay) {
            std::vector<JackTransport::InstrumentRouting> routings;
            for (const auto& ci : ui->app.outputsOverlay->getInstruments())
                routings.push_back({ci.id, ci.portName, ci.midiChannel,
                                    ci.programNumber, ci.bankMsb, ci.bankLsb});
            ui->jackTransport->setInstruments(routings);
        }
        if (!ui->restoringState) sendFullState(ui);
    };

    /* ---- Let File/Import and File/Export carry the outputs section ---- */
    ui->app.onCollectOutputs = [ui](AppState& state) { collectOverlayOutputs(ui, state); };
    ui->app.onApplyOutputs   = [ui](const AppState& state) {
        ui->restoringState = true;
        applyOverlayOutputs(ui, state);
        ui->restoringState = false;
        sendFullState(ui);
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

    /* ---- Try to connect to JACK for transport control ---- */
    JackTransport::silenceLogging();   /* keep the host's console clean */
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
    if (port_index != PORT_NOTIFY_OUT)
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
