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
#include "jackObserver.hpp"
#include "observableSong.hpp"
#include "observablePattern.hpp"
#include "observableInstrument.hpp"
#include "outputsOverlay.hpp"
#include "patternPanel.hpp"
#include "luvieApp.hpp"
#include "timelineIO.hpp"

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"
#define LUVIE_MIDI_URI  "https://github.com/benbriedis/luvie#AuditionMidi"

/* File used to pass state between DSP restore and UI open, and between UI sessions */
static std::string g_stateFilePath;

/* Plugin-mode transport: the playhead position and play state are *displayed* from
   the DSP-fed SimpleTransport, while transport *commands* (play / pause / rewind /
   seek) are sent to the JACK control client. Giving every UI widget a single
   ITransport that splits these two roles is what lets a UI seek actually relocate
   JACK/Ardour — otherwise the seek lands only in the display transport and the next
   DSP time:Position snaps it straight back. command is null while JACK is unavailable
   (commands become no-ops; the buttons are disabled separately). */
class HostTransport : public ITransport {
public:
    explicit HostTransport(ITransport* display) : display(display) {}
    void setCommand(ITransport* c) { command = c; }

    void  play()           override { if (command) command->play(); }
    void  pause()          override { if (command) command->pause(); }
    void  rewind()         override { if (command) command->rewind(); }
    void  seek(float bars) override { if (command) command->seek(bars); }
    float position()  const override { return display ? display->position()  : 0.0f; }
    bool  isPlaying() const override { return display ? display->isPlaying() : false; }
    void  setLoopMode(bool m) override { if (command) command->setLoopMode(m); }

private:
    ITransport* display = nullptr;
    ITransport* command = nullptr;
};

/* -----------------------------------------------------------------------
   Forward declarations
   ----------------------------------------------------------------------- */

struct LuvieUI;
static std::string serializeStateToString(LuvieUI* ui);
static void sendState(LuvieUI* ui);
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
    LV2_URID                luvie_midi_urid     = 0;
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

    /* Single transport handed to all UI widgets: displays the DSP position, routes
       commands (incl. playhead seeks) to jackControl. See HostTransport above. */
    HostTransport*       hostTransport   = nullptr;

    /* Transport-only JACK client: drives the JACK transport (start/stop/locate) so a
       host following JACK transport rolls. Used ONLY to send commands — the playhead
       position still comes from the DSP via the output port. jackObserver polls for the
       JACK server appearing/disappearing and enables/disables the UI buttons. */
    JackTransport*       jackControl     = nullptr;
    JackObserver*        jackObserver    = nullptr;

    /* UI layout (owns FLTK widgets via window) */
    LuvieApp            app;

    /* Set during state restore to suppress re-sending while rebuilding */
    bool restoringState = false;

    /* Incremented per full sendState() so the DSP worker can tell consecutive
       (chunked) messages apart and detect a dropped chunk. */
    uint32_t stateMsgId = 0;

    ~LuvieUI() {
        /* Stop the JACK poll (cancels its FLTK timeout) before tearing down; the
           observer references jackControl, so destroy it first. */
        if (jackObserver) jackObserver->stop();
        delete jackObserver;
        delete jackControl;
        /* app destructor removes timeline observers */
        /* Widgets are owned by window (added via add()), deleted with window */
        delete window;
        delete hostTransport;
        delete simpleTransport;
        delete pattern;
        delete instruments;
        delete song;
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
    if (!state.jackOutputs.empty())
        overlay->setOutputs(state.jackOutputs);  // JackOutput overload keeps backends
    std::vector<OutputsOverlay::InstrumentInfo> instrs;
    for (const auto& c : state.jackInstruments)
        instrs.push_back({c.id, c.name, c.portName, c.midiChannel, c.drumMap,
                          c.isDrum, c.fallbackNoteNames, c.programNumber, c.bankMsb, c.bankLsb,
                          c.gm1Instrument});
    overlay->setInstruments(instrs);
    ui->app.pushInstruments();
}

static bool buildAppState(LuvieUI* ui, AppState& state)
{
    if (!ui->song || !ui->app.patternPanel)
        return false;
    state.timeline  = ui->song->get();
    collectOverlayOutputs(ui, state);
    return true;
}

static std::string serializeStateToString(LuvieUI* ui)
{
    AppState state;
    if (!buildAppState(ui, state))
        return {};
    return appStateToJsonString(state);
}

/* Send the full project to the DSP as one or more luvie_state atoms on control_in.
   The JSON is split into fixed-size chunks (each a LuvieStateChunk header + payload
   slice) so a session larger than a host buffer still gets through; the DSP's run()
   forwards each chunk to the LV2 Worker, which reassembles, parses, and applies it
   to the real-time engine — this is how UI edits reach playback. (The per-process
   state file is only a load/restore handoff now; it is no longer written on every
   edit and the DSP no longer polls it.) */
static void sendState(LuvieUI* ui)
{
    if (!ui->writeFunc || !ui->luvie_state_urid || !ui->atom_eventTransfer)
        return;
    std::string json = serializeStateToString(ui);
    if (json.empty()) return;

    /* Conservative payload size: small enough to fit any reasonable host atom-port
       and worker buffer, so chunking — not buffer size — is what bounds a session. */
    constexpr uint32_t kChunkPayload = 8192;
    const uint32_t total = static_cast<uint32_t>(json.size());
    const uint32_t msgId = ++ui->stateMsgId;

    std::vector<uint8_t> buf;
    for (uint32_t offset = 0; offset < total; offset += kChunkPayload) {
        const uint32_t chunkSize = std::min(kChunkPayload, total - offset);

        /* eventTransfer payload is a complete atom: header + (LuvieStateChunk + slice). */
        buf.resize(sizeof(LV2_Atom) + sizeof(LuvieStateChunk) + chunkSize);
        auto* atom = reinterpret_cast<LV2_Atom*>(buf.data());
        atom->size = static_cast<uint32_t>(sizeof(LuvieStateChunk) + chunkSize);
        atom->type = ui->luvie_state_urid;

        LuvieStateChunk hdr{ msgId, total, offset, chunkSize };
        std::memcpy(buf.data() + sizeof(LV2_Atom), &hdr, sizeof(hdr));
        std::memcpy(buf.data() + sizeof(LV2_Atom) + sizeof(LuvieStateChunk),
                    json.data() + offset, chunkSize);

        ui->writeFunc(ui->controller, PORT_CONTROL_IN,
                      static_cast<uint32_t>(buf.size()),
                      ui->atom_eventTransfer, buf.data());
    }
}

/* Send one raw MIDI message (1-3 bytes) to the DSP as a `luvie_midi` atom on
   control_in; the DSP re-emits it on midi_out this cycle. Used to audition a note
   when a row label is clicked — the plugin equivalent of the standalone's direct
   port write (there is no local PortRegistry when hosted). */
static void sendAuditionMidi(LuvieUI* ui, const uint8_t* bytes, int len)
{
    if (!ui->writeFunc || !ui->luvie_midi_urid || !ui->atom_eventTransfer) return;
    if (len < 1 || len > 3) return;

    uint8_t buf[sizeof(LV2_Atom) + 3];
    auto* atom = reinterpret_cast<LV2_Atom*>(buf);
    atom->size = static_cast<uint32_t>(len);
    atom->type = ui->luvie_midi_urid;
    std::memcpy(buf + sizeof(LV2_Atom), bytes, len);

    ui->writeFunc(ui->controller, PORT_CONTROL_IN,
                  static_cast<uint32_t>(sizeof(LV2_Atom) + len),
                  ui->atom_eventTransfer, buf);
}

static void deserializeFullState(LuvieUI* ui, const uint8_t* data, uint32_t size)
{
    if (!ui->song || !ui->app.patternPanel || size == 0) return;
    AppState state;
    if (!appStateFromJsonString(std::string(reinterpret_cast<const char*>(data), size), state)) {
        fprintf(stderr, "[luvie_ui] deserializeFullState: JSON parse failed\n");
        return;
    }
    /* A valid Luvie session always has at least one track — the UI makes it
       impossible to delete the last one. An empty timeline therefore means there
       is nothing meaningful to restore (a stale or empty state file, e.g. one
       saved before the default tracks were seeded). Applying it would wipe the
       defaults that build() just seeded and leave the Song Editor with zero
       tracks, so skip it. */
    if (state.timeline.tracks.empty()) {
        fprintf(stderr, "[luvie_ui] deserializeFullState: ignoring empty timeline\n");
        return;
    }
    ui->restoringState = true;
    ui->song->loadTimeline(state.timeline);
    applyOverlayOutputs(ui, state);
    ui->restoringState = false;
    /* No sendState() here: this is called from the restore path, where the DSP
       already applied exactly this content in dsp_restore. Re-sending it would only
       schedule a redundant worker parse. */
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
        ui->luvie_midi_urid     = m(LUVIE_MIDI_URI);
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
    ui->hostTransport = new HostTransport(ui->simpleTransport);

    /* ---- LuvieApp callbacks ---- */
    ui->app.disableTransportButtons = true;
    ui->app.pluginMode              = true;

    /* Row-label auditioning: there is no local PortRegistry when hosted, so resolve
       the clicked instrument to its MIDI channel (the overlay exists after build();
       the lambda reads it lazily at click time) and forward the note to the DSP,
       which emits it on midi_out. */
    ui->app.instrRoute = [ui](int instrumentId) -> MidiInstrRoute {
        if (auto* ov = ui->app.outputsOverlay)
            for (const auto& ci : ov->getInstruments())
                if (ci.id == instrumentId) return { ci.portName, ci.midiChannel - 1 };
        return {};
    };

    ui->app.onExtraTimelineChange = [ui]() {
        if (!ui->restoringState) sendState(ui);
    };

    /* Transport is host-driven in plugin mode (the engine follows JACK transport),
       so UI seeks are not propagated to the engine — the host's next time:Position
       resyncs the playhead display anyway. */

    ui->app.onExtraParamsChanged = [ui]() {
        if (!ui->restoringState) sendState(ui);
    };

    /* ---- Build all shared UI ---- */
    /* Suppress state-file writes while build() seeds its default tracks: the DSP
       may already have restored a project to the state file (dsp_restore), and a
       default-seeding write here would clobber it before the restore block below
       reads it back. */
    ui->restoringState = true;
    ui->app.build(ui->window, ui->song, ui->pattern, ui->instruments, ui->hostTransport);
    ui->app.disableSaveMenu(/*saveAs=*/true);

    /* Emit auditioned notes (and their note-offs) to the DSP as raw MIDI. */
    ui->app.auditioner.setMidiSink([ui](int ch, int midi, int vel, bool on) {
        uint8_t m[3] = {
            static_cast<uint8_t>((on ? 0x90 : 0x80) | (ch & 0x0F)),
            static_cast<uint8_t>(midi & 0x7F),
            static_cast<uint8_t>(on ? (vel & 0x7F) : 0),
        };
        sendAuditionMidi(ui, m, 3);
    });

    /* ---- JACK transport control (Issue #2) ----
       The transport buttons start disabled (disableTransportButtons above). When a
       JACK server is available, route them through a transport-only JACK client so
       pressing Play here starts the JACK transport — a host set to follow JACK
       transport then rolls and, via its time:Position, the DSP plays. The buttons are
       re-disabled (with an alert) whenever JACK is unavailable. The poll runs on the
       FLTK timer pumped by ui_run()'s Fl::check(). */
    JackTransport::silenceLogging();
    ui->jackControl = new JackTransport;
    ui->jackControl->setTimeline(ui->song);                 // for seek() bar->frame
    ui->jackControl->awakeFn = [](void (*f)(void*), void* d) { Fl::awake(f, d); };
    ui->jackControl->onShutdown = [ui]() { ui->jackObserver->serverLost(); };

    ui->jackObserver = new JackObserver(ui->jackControl);
    ui->jackObserver->addListener([ui](JackObserver::State s) {
        Transport* bp = ui->app.bottomPane;
        if (!bp) return;
        if (s == JackObserver::State::Up) {
            ui->hostTransport->setCommand(ui->jackControl);   // route commands to JACK
            bp->enableButtons();
            bp->setAlerts({});
            bp->syncPlayState();
        } else {
            ui->hostTransport->setCommand(nullptr);           // commands become no-ops
            bp->disableButtons();
            bp->setAlerts({"JACK unavailable — transport control disabled"});
        }
    });
    ui->jackObserver->start();

    /* ---- Wire port management ---- */
    if (auto* overlay = ui->app.outputsOverlay) {
        /* No new-project dialog in the plugin; default MIDI output is Jack. The DSP
           engine reconciles its JACK ports from the project state, so every change
           just re-sends the current state atom. */
        overlay->setDefaultBackend(MidiBackend::Jack);
        overlay->onPortAdded   = [ui](const std::string&) { if (!ui->restoringState) sendState(ui); };
        overlay->onPortRemoved = [ui](const std::string&) { if (!ui->restoringState) sendState(ui); };
        overlay->onPortRenamed = [ui](const std::string&, const std::string&) { if (!ui->restoringState) sendState(ui); };
    }

    /* ---- Persist instrument-routing changes for the DSP engine ---- */
    ui->app.onInstrumentsChanged = [ui]() {
        if (!ui->restoringState) sendState(ui);
    };

    /* ---- Let File/Import and File/Export carry the outputs section ---- */
    ui->app.onCollectOutputs = [ui](AppState& state) { collectOverlayOutputs(ui, state); };
    ui->app.onApplyOutputs   = [ui](const AppState& state) {
        ui->restoringState = true;
        applyOverlayOutputs(ui, state);
        ui->restoringState = false;
        sendState(ui);
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

    /* Done restoring (deserializeFullState clears this too, but not when there was
       no file). Then send the current project once so the DSP engine picks it up —
       whether restored or freshly seeded defaults. */
    ui->restoringState = false;
    sendState(ui);

    /* Return the widget pointer as the LV2UI handle */
    *widget = &ui->widget;
    return reinterpret_cast<LV2UI_Handle>(ui);
}

static void cleanup(LV2UI_Handle handle)
{
    LuvieUI* ui = reinterpret_cast<LuvieUI*>(handle);

    /* Save current state to file so the next instantiate can restore it.
       Never persist an empty (zero-track) session: a valid session always has at
       least one track, so an empty one would only be a transient/uninitialised
       state. Writing it would poison the file and make the next open show no
       tracks (see deserializeFullState). */
    if (!g_stateFilePath.empty() && ui->song && !ui->song->get().tracks.empty()) {
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
    if (port_index != PORT_OUT)
        return;

    const LV2_Atom* atom = static_cast<const LV2_Atom*>(buffer);

    /* The single output port carries time:Position (for the playhead), the MIDI
       events going to the instrument, and state:StateChanged (for the host). Only
       the Position concerns the UI; MIDI and StateChanged atoms fall through the
       type checks below and are ignored. */
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
