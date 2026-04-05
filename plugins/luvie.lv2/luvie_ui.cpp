#include "lv2_external_ui.h"
#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>

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
#include "../../src/markerPopup.hpp"
#include "../../src/markerRuler.hpp"
#include "../../src/observableTimeline.hpp"
#include "../../src/patternPanel.hpp"
#include "../../src/trackContextPopup.hpp"
#include "../../src/noteLabels.hpp"
#include "../../src/chords.hpp"
#include "timeline_serial.h"

/* -----------------------------------------------------------------------
   Forward declarations
   ----------------------------------------------------------------------- */

struct LuvieUI;
static void sendTimeline(LuvieUI* ui);

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

    /* Non-owning pointer — widget is owned by FLTK/window */
    PatternPanel*       patternPanel   = nullptr;

    /* Popups — stored as members so they live long enough */
    Popup*              popup1         = nullptr;
    Popup*              popup2         = nullptr;
    MarkerPopup*        tempoPopup     = nullptr;
    MarkerPopup*        timeSigPopup   = nullptr;
    TrackContextPopup*  trackCtxPopup  = nullptr;

    /* Observer that forwards timeline changes to the DSP */
    struct TimelineSender : ITimelineObserver {
        LuvieUI* ui;
        explicit TimelineSender(LuvieUI* u) : ui(u) {}
        void onTimelineChanged() override { sendTimeline(ui); }
    } timelineSender{this};

    ~LuvieUI() {
        if (songTimeline) songTimeline->removeObserver(&timelineSender);
        /* Popups are added to window via add(), so window owns and deletes them. */
        delete window;
        delete simpleTransport;
        delete songTimeline;
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

    if (ui->map) {
        auto m = [&](const char* uri) { return ui->map->map(ui->map->handle, uri); };
        ui->atom_eventTransfer  = m(LV2_ATOM__eventTransfer);
        ui->luvie_timeline_urid = m(LUVIE_TIMELINE_URI);
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
    ModernTabs* tabs = new ModernTabs(0, 0, winW, tabsH);
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

    /* ---- Pattern Editor tab ---- */
    const int panelH   = 50;
    const int rowHeight = 30;
    const int numRows   = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / rowHeight;

    Fl_Group* tab2 = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab2->color(bgColor);
    tabs->add(*tab2);

    std::vector<Note> notes2;
    PatternEditor* og1 = new PatternEditor(0, tabBarH, winW, notes2, numRows,
                                           numPatternBeats, rowHeight, 40, 0.25, *ui->popup1);
    tab2->add(og1);

    PatternPanel* patternPanel = new PatternPanel(0, tabsH - panelH, winW, panelH);
    patternPanel->setTimeline(ui->songTimeline);
    tab2->add(patternPanel);
    tab2->end();

    ui->patternPanel = patternPanel;

    /* ---- Transport ---- */
    Fl_Group::current(nullptr);
    ui->simpleTransport = new SimpleTransport;
    ui->simpleTransport->setTimeline(ui->songTimeline);
    Transport* bottomPane = new Transport(0, tabsH, winW, bottomH,
                                          ui->simpleTransport, ui->songTimeline);
    ui->window->add(bottomPane);

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
    og2->onSeek       = [bottomPane]() { bottomPane->notifySeek(); };
    og2->onPatternDoubleClick = [tabs, tab2](int /*trackIndex*/) {
        tabs->value(tab2);
        tabs->redraw();
    };

    og1->setPatternPlayhead(ui->simpleTransport, ui->songTimeline, 0);
    ui->songTimeline->selectTrack(0);

    auto syncNoteLabels = [og1, patternPanel]() {
        og1->setNoteParams(patternPanel->rootPitch(),
                           patternPanel->chordType(),
                           patternPanel->isSharp());
    };
    patternPanel->onParamsChanged = [ui, syncNoteLabels]() {
        syncNoteLabels();
        sendTimeline(ui);
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
    tab2->resizable(og1);
    ui->window->resizable(tabs);

    const int minW = 14 + 36 + 5*40;
    const int minH = tabBarH + Editor::rulerH + 5*rowHeight + Editor::hScrollH + panelH + bottomH;
    ui->window->size_range(minW, minH);

    /* ---- Register timeline observer and send initial state to DSP ---- */
    ui->songTimeline->addObserver(&ui->timelineSender);
    sendTimeline(ui);

    /* Return the widget pointer as the LV2UI handle */
    *widget = &ui->widget;
    return reinterpret_cast<LV2UI_Handle>(ui);
}

static void cleanup(LV2UI_Handle handle)
{
    delete reinterpret_cast<LuvieUI*>(handle);
}

static void port_event(LV2UI_Handle handle, uint32_t port_index,
                       uint32_t /*buffer_size*/, uint32_t /*format*/,
                       const void* buffer)
{
    LuvieUI* ui = reinterpret_cast<LuvieUI*>(handle);
    if (port_index != 2)
        return;

    const LV2_Atom* atom = static_cast<const LV2_Atom*>(buffer);
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
