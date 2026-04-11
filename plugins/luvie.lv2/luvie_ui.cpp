#include "lv2_external_ui.h"
#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <cstdio>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

#include "../../src/appWindow.hpp"
#include "../../src/songEditor.hpp"
#include "../../src/patternEditor.hpp"
#include "../../src/popup.hpp"
#include "../../src/modernTabs.hpp"
#include "../../src/transport.hpp"
#include "../../src/simpleTransport.hpp"
#include "../../src/jackTransport.hpp"
#include "../../src/markerPopup.hpp"
#include "../../src/markerRuler.hpp"
#include "../../src/observableTimeline.hpp"
#include "../../src/patternPanel.hpp"
#include "../../src/trackContextPopup.hpp"
#include "../../src/noteLabels.hpp"
#include "../../src/chords.hpp"
#include "../../src/loopEditor.hpp"
#include "../../src/drumPatternEditor.hpp"
#include "timeline_serial.h"
#include <nlohmann/json.hpp>

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"

/* File used to pass state between DSP restore and UI open, and between UI sessions */
static std::string g_stateFilePath;

using json = nlohmann::json;

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
    ObservableTimeline* songTimeline   = nullptr;
    SimpleTransport*    simpleTransport= nullptr;

    /* Non-owning pointers — widgets are owned by FLTK/window */
    PatternEditor*      og1            = nullptr;
    PatternPanel*       patternPanel   = nullptr;
    Transport*          bottomPane     = nullptr;
    LoopEditor*         loopEd         = nullptr;
    DrumPatternEditor*  drumEd         = nullptr;

    /* JACK transport control (nullptr when JACK is unavailable) */
    JackTransport*      jackTransport  = nullptr;

    /* Popups — stored as members so they live long enough */
    Popup*              popup1         = nullptr;
    Popup*              popup2         = nullptr;
    MarkerPopup*        tempoPopup     = nullptr;
    MarkerPopup*        timeSigPopup   = nullptr;
    TrackContextPopup*  trackCtxPopup  = nullptr;

    /* Set during state restore to suppress re-sending while rebuilding */
    bool restoringState = false;

    /* Switches between standard and drum editors when selected track changes */
    struct EditorSwitcher : ITimelineObserver {
        LuvieUI* ui;
        explicit EditorSwitcher(LuvieUI* u) : ui(u) {}
        void onTimelineChanged() override {
            if (!ui->songTimeline || !ui->og1 || !ui->drumEd) return;
            const auto& data = ui->songTimeline->get();
            int sel = data.selectedTrackIndex;
            bool isDrum = false;
            if (sel >= 0 && sel < (int)data.tracks.size()) {
                int patId = data.tracks[sel].patternId;
                for (const auto& p : data.patterns)
                    if (p.id == patId) { isDrum = (p.type == PatternType::DRUM); break; }
            }
            if (isDrum) { ui->og1->hide(); ui->drumEd->show(); }
            else        { ui->og1->show(); ui->drumEd->hide(); }
        }
    } editorSwitcher{this};

    /* Observer that forwards timeline changes to the DSP */
    struct TimelineSender : ITimelineObserver {
        LuvieUI* ui;
        explicit TimelineSender(LuvieUI* u) : ui(u) {}
        void onTimelineChanged() override {
            if (ui->restoringState) return;
            sendTimeline(ui);
            sendFullState(ui);
        }
    } timelineSender{this};

    ~LuvieUI() {
        if (songTimeline) {
            songTimeline->removeObserver(&timelineSender);
            songTimeline->removeObserver(&editorSwitcher);
        }
        /* Popups are added to window via add(), so window owns and deletes them. */
        delete window;
        delete jackTransport;
        delete simpleTransport;
        delete songTimeline;
    }

    /* Try once to open JACK (control-only, no MIDI). Enables buttons on success,
       leaves them disabled if JACK is not available. */
    void tryConnectJack() {
        auto* jt = new JackTransport();
        if (jt->open("luvie_lv2", /*enableMidi=*/false)) {
            jackTransport = jt;
            jackTransport->setTimeline(songTimeline);
            jackTransport->onTransportEvent = [this]() {
                Fl::awake([](void* data) {
                    static_cast<LuvieUI*>(data)->bottomPane->syncPlayState();
                }, this);
            };
            if (bottomPane) bottomPane->setControlTransport(jackTransport);
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
    if (!ui->writeFunc || !ui->songTimeline || !ui->patternPanel)
        return;

    const Timeline& tl        = ui->songTimeline->get();
    const int rootPitch        = ui->patternPanel->rootPitch();
    const int chordType        = ui->patternPanel->chordType();
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

    /* Tracks */
    for (const auto& track : tl.tracks) {
        SerialTrackHeader sth = { track.id, 0, {0,0,0},
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
    if (!ui->songTimeline || !ui->patternPanel)
        return {};

    const Timeline& tl = ui->songTimeline->get();

    json j;
    j["version"]            = 1;
    j["rootPitch"]          = ui->patternPanel->rootPitch();
    j["chordType"]          = ui->patternPanel->chordType();
    j["useSharp"]           = ui->patternPanel->isSharp();
    j["selectedTrackIndex"] = tl.selectedTrackIndex;

    json timeSigs = json::array();
    for (const auto& ts : tl.timeSigs)
        timeSigs.push_back({{"bar", ts.bar}, {"top", ts.top}, {"bottom", ts.bottom}});
    j["timeSigs"] = timeSigs;

    json bpms = json::array();
    for (const auto& bm : tl.bpms)
        bpms.push_back({{"bar", bm.bar}, {"bpm", bm.bpm}});
    j["bpms"] = bpms;

    json patterns = json::array();
    for (const auto& pat : tl.patterns) {
        json notes = json::array();
        for (const auto& n : pat.notes)
            notes.push_back({
                {"id",             n.id},
                {"pitch",          n.pitch},
                {"beat",           n.beat},
                {"length",         n.length},
                {"velocity",       n.velocity},
                {"disabled",       n.disabled},
                {"disabledDegree", n.disabledDegree}
            });

        json drumNotes = json::array();
        for (const auto& dn : pat.drumNotes)
            drumNotes.push_back({
                {"id",       dn.id},
                {"note",     dn.note},
                {"beat",     dn.beat},
                {"velocity", dn.velocity}
            });

        patterns.push_back({
            {"id",          pat.id},
            {"lengthBeats", pat.lengthBeats},
            {"type",        (int)pat.type},
            {"notes",       notes},
            {"drumNotes",   drumNotes}
        });
    }
    j["patterns"] = patterns;

    json tracks = json::array();
    for (const auto& track : tl.tracks) {
        json instances = json::array();
        for (const auto& inst : track.patterns)
            instances.push_back({
                {"id",          inst.id},
                {"patternId",   inst.patternId},
                {"startBar",    inst.startBar},
                {"length",      inst.length},
                {"startOffset", inst.startOffset}
            });

        tracks.push_back({
            {"id",        track.id},
            {"label",     track.label},
            {"patternId", track.patternId},
            {"instances", instances}
        });
    }
    j["tracks"] = tracks;

    return j.dump();
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
    if (!ui->songTimeline || !ui->patternPanel || size == 0) return;

    json j;
    try {
        j = json::parse(data, data + size);
    } catch (...) {
        fprintf(stderr, "[luvie_ui] deserializeFullState: JSON parse failed\n");
        return;
    }

    if (j.value("version", 0) != 1) {
        fprintf(stderr, "[luvie_ui] deserializeFullState: bad version\n");
        return;
    }

    Timeline tl;
    tl.selectedTrackIndex = j.value("selectedTrackIndex", -1);

    for (const auto& ts : j.value("timeSigs", json::array()))
        tl.timeSigs.push_back({ts["bar"], ts["top"], ts["bottom"]});

    for (const auto& bm : j.value("bpms", json::array()))
        tl.bpms.push_back({bm["bar"], bm.value("bpm", 120.0f)});

    for (const auto& p : j.value("patterns", json::array())) {
        Pattern pat;
        pat.id          = p["id"];
        pat.lengthBeats = p.value("lengthBeats", 8.0f);
        pat.type        = (PatternType)p.value("type", 0);
        for (const auto& n : p.value("notes", json::array())) {
            Note note;
            note.id             = n["id"];
            note.pitch          = n.value("pitch", 0);
            note.beat           = n.value("beat", 0.0f);
            note.length         = n.value("length", 1.0f);
            note.velocity       = n.value("velocity", 0.0f);
            note.disabled       = n.value("disabled", false);
            note.disabledDegree = n.value("disabledDegree", -1);
            pat.notes.push_back(note);
        }
        for (const auto& dn : p.value("drumNotes", json::array())) {
            DrumNote drumNote;
            drumNote.id       = dn["id"];
            drumNote.note     = dn.value("note", 0);
            drumNote.beat     = dn.value("beat", 0.0f);
            drumNote.velocity = dn.value("velocity", 0.8f);
            pat.drumNotes.push_back(drumNote);
        }
        tl.patterns.push_back(std::move(pat));
    }

    for (const auto& t : j.value("tracks", json::array())) {
        Track track;
        track.id        = t["id"];
        track.label     = t.value("label", "");
        track.patternId = t.value("patternId", 0);
        for (const auto& inst : t.value("instances", json::array())) {
            PatternInstance pi;
            pi.id          = inst["id"];
            pi.patternId   = inst.value("patternId", 0);
            pi.startBar    = inst.value("startBar", 0.0f);
            pi.length      = inst.value("length", 4.0f);
            pi.startOffset = inst.value("startOffset", 0.0f);
            track.patterns.push_back(pi);
        }
        tl.tracks.push_back(std::move(track));
    }

    /* Apply restored state without triggering intermediate sends */
    ui->restoringState = true;
    ui->patternPanel->setParams(
        j.value("rootPitch", 0),
        j.value("chordType", 0),
        j.value("useSharp", true));
    ui->songTimeline->loadTimeline(tl);
    ui->restoringState = false;

    /* Now send the correct state to the DSP */
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

    /* ---- Layout constants (same as standalone main.cpp) ---- */
    const int tabBarH      = 35;
    const int bottomH      = 50;
    const int markerRulerH = 18;
    const int winW         = 920;
    const int winH         = tabBarH + 2*markerRulerH + Editor::rulerH + 10*45 + 20 + bottomH;

    /* ---- Window ---- */
    const char* title = (ui->extHost && ui->extHost->plugin_human_id)
                        ? ui->extHost->plugin_human_id : "Luvie";
    ui->window = new AppWindow(winW, winH);
    ui->window->label(title);
    ui->window->color(bgColor);
    ui->window->callback(window_close_cb, ui);
    ui->window->end();   /* don't auto-add children until we call begin() */

    /* ---- Popups ---- */
    ui->popup1       = new Popup{};
    ui->popup2       = new Popup{};
    ui->tempoPopup   = new MarkerPopup(MarkerPopup::TEMPO);
    ui->timeSigPopup = new MarkerPopup(MarkerPopup::TIME_SIG);
    ui->trackCtxPopup= new TrackContextPopup;

    const int tabsH = winH - bottomH;

    /* ---- Tabs ---- */
    static constexpr Fl_Color songColor = 0x22C55E00;
    static constexpr Fl_Color loopColor = 0x3B82F600;

    ModernTabs* tabs = new ModernTabs(0, 0, winW, tabsH);
    tabs->enableModeToggle(songColor, loopColor);
    tabs->setTabAccent(0, songColor);
    tabs->setTabAccent(1, loopColor);
    tabs->setTabAccent(2, 0xF9731600);
    ui->window->add(tabs);

    /* ---- Song Editor tab ---- */
    Fl_Group* tab1 = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Song Editor");
    tab1->color(bgColor);
    tabs->add(*tab1);

    const int numPatternBeats = 8;

    ui->songTimeline = new ObservableTimeline(120.0f, 4, 4);
    for (int i = 1; i <= 8; i++) {
        int patId = ui->songTimeline->createPattern(numPatternBeats);
        ui->songTimeline->addTrack("Pattern " + std::to_string(i), patId);
    }

    MarkerRuler* timeSigRuler = new MarkerRuler(0, tabBarH, winW, markerRulerH,
        80, 60, MarkerRuler::TIME_SIG, ui->songTimeline, ui->timeSigPopup);
    tab1->add(timeSigRuler);

    MarkerRuler* tempoRuler = new MarkerRuler(0, tabBarH + markerRulerH, winW, markerRulerH,
        80, 60, MarkerRuler::TEMPO, ui->songTimeline, ui->tempoPopup);
    tab1->add(tempoRuler);

    TrackContextPopup* trackCtxPopup = ui->trackCtxPopup;

    std::vector<Note> patterns2;
    SongEditor* og2 = new SongEditor(0, tabBarH + 2*markerRulerH, winW, patterns2,
                                     10, 80, 45, 60, 0.25, *ui->popup2);
    tab1->add(og2);
    tab1->end();

    /* ---- Loop Editor tab ---- */
    Fl_Group* tabLoop = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Loop Editor");
    tabLoop->color(bgColor);
    tabs->add(*tabLoop);

    LoopEditor* loopEd = new LoopEditor(0, tabBarH, winW, tabsH - tabBarH);
    tabLoop->add(loopEd);
    tabLoop->end();

    ui->loopEd = loopEd;

    /* ---- Pattern Editor tab ---- */
    const int panelH      = 50;
    const int rowHeight   = 30;
    const int numRows     = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / rowHeight;
    const int drumRowH    = 20;
    const int drumNumRows = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / drumRowH;

    Fl_Group* tab2 = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab2->color(bgColor);
    tabs->add(*tab2);

    std::vector<Note> notes2;
    PatternEditor* og1 = new PatternEditor(0, tabBarH, winW, notes2, numRows,
                                           numPatternBeats, rowHeight, 40, 0.25, *ui->popup1);
    tab2->add(og1);

    DrumPatternEditor* drumEd = new DrumPatternEditor(0, tabBarH, winW, drumNumRows,
                                                      numPatternBeats, drumRowH, 40, 0.25f, *ui->popup1);
    tab2->add(drumEd);
    drumEd->hide();

    PatternPanel* patternPanel = new PatternPanel(0, tabsH - panelH, winW, panelH);
    patternPanel->setTimeline(ui->songTimeline);
    tab2->add(patternPanel);
    tab2->end();

    ui->og1         = og1;
    ui->drumEd      = drumEd;
    ui->patternPanel = patternPanel;

    /* ---- Transport ---- */
    Fl_Group::current(nullptr);
    ui->simpleTransport = new SimpleTransport;
    ui->simpleTransport->setTimeline(ui->songTimeline);
    Transport* bottomPane = new Transport(0, tabsH, winW, bottomH,
                                          ui->simpleTransport, ui->songTimeline);
    ui->window->add(bottomPane);
    ui->bottomPane = bottomPane;
    bottomPane->disableButtons();  // enabled only when JACK connects

    /* ---- Wire up editors ---- */
    og2->setTransport(ui->simpleTransport, ui->songTimeline);
    og2->onRulerOffsetChanged = [timeSigRuler, tempoRuler](int off, int clipLeft) {
        timeSigRuler->setOffsetX(off);
        timeSigRuler->setClipLeft(clipLeft);
        tempoRuler->setOffsetX(off);
        tempoRuler->setClipLeft(clipLeft);
    };
    og2->setContextPopup(trackCtxPopup);
    og2->onEndReached = [bottomPane]() { bottomPane->notifyEndReached(); };
    og2->onSeek       = [bottomPane, ui]() {
        bottomPane->notifySeek();
        if (ui->jackTransport)
            ui->jackTransport->seek(ui->simpleTransport->position());
    };
    og2->onPatternDoubleClick = [tabs, tab2](int /*trackIndex*/) {
        tabs->value(tab2);
        tabs->redraw();
    };

    loopEd->setTimeline(ui->songTimeline);
    loopEd->setTransport(ui->simpleTransport);
    loopEd->setContextPopup(trackCtxPopup);

    og1->setPatternPlayhead(ui->simpleTransport, ui->songTimeline, 0);
    drumEd->setPatternPlayhead(ui->simpleTransport, ui->songTimeline, 0);
    ui->songTimeline->selectTrack(0);

    auto syncNoteLabels = [og1, patternPanel]() {
        og1->setNoteParams(patternPanel->rootPitch(),
                           patternPanel->chordType(),
                           patternPanel->isSharp());
    };
    patternPanel->onParamsChanged = [ui, syncNoteLabels]() {
        syncNoteLabels();
        if (!ui->restoringState) {
            sendTimeline(ui);
            sendFullState(ui);
        }
    };
    syncNoteLabels();

    /* ---- Popups must be added last (reverse-order FLTK dispatch) ---- */
    ui->window->add(ui->popup1);        ui->window->registerPopup(ui->popup1);
    ui->window->add(ui->popup2);        ui->window->registerPopup(ui->popup2);
    ui->window->add(ui->tempoPopup);    ui->window->registerPopup(ui->tempoPopup);
    ui->window->add(ui->timeSigPopup);  ui->window->registerPopup(ui->timeSigPopup);
    ui->window->add(ui->trackCtxPopup); ui->window->registerPopup(ui->trackCtxPopup);

    /* ---- Resizable chain ---- */
    tab1->resizable(og2);
    tabLoop->resizable(loopEd);
    tab2->resizable(og1);
    ui->window->resizable(tabs);

    const int minW = 14 + 36 + 5*40;
    const int minH = tabBarH + Editor::rulerH + 5*rowHeight + Editor::hScrollH + panelH + bottomH;
    ui->window->size_range(minW, minH);

    /* ---- Register timeline observer and send initial state to DSP ---- */
    ui->songTimeline->addObserver(&ui->timelineSender);
    ui->songTimeline->addObserver(&ui->editorSwitcher);

    /* Restore from file if available (written by dsp_restore or previous UI close) */
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
    if (ui->bottomPane) ui->bottomPane->syncPlayState();
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
