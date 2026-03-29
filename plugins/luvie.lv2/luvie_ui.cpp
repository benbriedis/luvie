#include "lv2_external_ui.h"
#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>

#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <string>
#include <cstring>

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

/* -----------------------------------------------------------------------
   LuvieUI — the ExternalUI instance.  The struct's first member is
   LV2_External_UI_Widget, so a LuvieUI* casts cleanly to the widget type
   that the host expects back from instantiate().
   ----------------------------------------------------------------------- */

struct LuvieUI {
    /* Must be first — the host casts our handle to this type. */
    LV2_External_UI_Widget widget;

    LV2UI_Controller        controller = nullptr;
    LV2_External_UI_Host*   extHost    = nullptr;

    /* Owned heap objects */
    AppWindow*          window         = nullptr;
    ObservableTimeline* songTimeline   = nullptr;
    SimpleTransport*    simpleTransport= nullptr;

    /* Popups — stored as members so they live long enough */
    Popup*              popup1         = nullptr;
    Popup*              popup2         = nullptr;
    MarkerPopup*        tempoPopup     = nullptr;
    MarkerPopup*        timeSigPopup   = nullptr;
    TrackContextPopup*  trackCtxPopup  = nullptr;

    ~LuvieUI() {
        /* Fl_Widget children are owned by the window; delete window first. */
        delete window;
        delete popup1;
        delete popup2;
        delete tempoPopup;
        delete timeSigPopup;
        delete trackCtxPopup;
        delete simpleTransport;
        delete songTimeline;
    }
};

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
    LV2UI_Write_Function      /*write_function*/,
    LV2UI_Controller          controller,
    LV2UI_Widget*             widget,
    const LV2_Feature* const* features)
{
    LuvieUI* ui = new LuvieUI;
    ui->widget.run  = ui_run;
    ui->widget.show = ui_show;
    ui->widget.hide = ui_hide;
    ui->controller  = controller;

    /* Scan features for ExternalUI host */
    for (int i = 0; features[i]; i++) {
        if (std::strcmp(features[i]->URI, LV2_EXTERNAL_UI_HOST_URI) == 0)
            ui->extHost = static_cast<LV2_External_UI_Host*>(features[i]->data);
        else if (std::strcmp(features[i]->URI, LV2_EXTERNAL_UI_DEPRECATED_URI) == 0
                 && !ui->extHost)
            ui->extHost = static_cast<LV2_External_UI_Host*>(features[i]->data);
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

    /* ---- Transport ---- */
    /* Clear current group so Transport doesn't auto-add to tabs (Fl_Tabs would
       hide it as a non-active child, and the hidden flag would persist after
       reparenting to the window). */
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
    patternPanel->onParamsChanged = syncNoteLabels;
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

    /* Return the widget pointer as the LV2UI handle */
    *widget = &ui->widget;
    return reinterpret_cast<LV2UI_Handle>(ui);
}

static void cleanup(LV2UI_Handle handle)
{
    delete reinterpret_cast<LuvieUI*>(handle);
}

static void port_event(LV2UI_Handle /*handle*/, uint32_t /*port_index*/,
                       uint32_t /*buffer_size*/, uint32_t /*format*/,
                       const void* /*buffer*/)
{
    /* TODO: receive timeline state from DSP */
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
