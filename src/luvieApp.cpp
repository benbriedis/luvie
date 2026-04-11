#include "luvieApp.hpp"
#include "observableTimeline.hpp"
#include "appWindow.hpp"
#include "songEditor.hpp"
#include "patternEditor.hpp"
#include "popup.hpp"
#include "modernTabs.hpp"
#include "transport.hpp"
#include "markerPopup.hpp"
#include "markerRuler.hpp"
#include "patternPanel.hpp"
#include "trackContextPopup.hpp"
#include "drumPatternEditor.hpp"
#include "loopEditor.hpp"

void LuvieApp::EditorSwitcher::onTimelineChanged() {
    if (!app->patternEd || !app->drumEd || !app->timeline_) return;
    const auto& data = app->timeline_->get();
    int sel = data.selectedTrackIndex;
    bool isDrum = false;
    if (sel >= 0 && sel < (int)data.tracks.size()) {
        int patId = data.tracks[sel].patternId;
        for (const auto& p : data.patterns)
            if (p.id == patId) { isDrum = (p.type == PatternType::DRUM); break; }
    }
    if (isDrum) { app->patternEd->hide(); app->drumEd->show(); }
    else        { app->patternEd->show(); app->drumEd->hide(); }
}

void LuvieApp::build(AppWindow* window, ObservableTimeline* timeline, ITransport* transport) {
    timeline_ = timeline;

    const int tabsH      = defaultWinH() - bottomH;
    const int drumRowH   = 20;
    const int numRows     = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / rowHeight;
    const int drumNumRows = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / drumRowH;

    // Reset current group so nothing auto-parents unexpectedly
    Fl_Group::current(nullptr);

    // ---- Popups (created before any group so they stay unparented until explicit add) ----
    auto* p1     = new Popup{};
    auto* p2     = new Popup{};
    auto* tPop   = new MarkerPopup(MarkerPopup::TEMPO);
    auto* tsPop  = new MarkerPopup(MarkerPopup::TIME_SIG);
    auto* ctxPop = new TrackContextPopup;

    // ---- Tabs ----
    static constexpr Fl_Color songColor = 0x22C55E00;
    static constexpr Fl_Color loopColor = 0x3B82F600;

    tabs = new ModernTabs(0, 0, winW, tabsH);
    tabs->end();
    tabs->enableModeToggle(songColor, loopColor);
    tabs->setTabAccent(0, songColor);
    tabs->setTabAccent(1, loopColor);
    tabs->setTabAccent(2, 0xF9731600);
    window->add(tabs);

    // ---- Song Editor tab ----
    auto* tab1 = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Song Editor");
    tab1->end();
    tab1->color(bgColor);

    auto* timeSigRuler = new MarkerRuler(0, tabBarH, winW, markerRulerH,
        80, 60, MarkerRuler::TIME_SIG, timeline, tsPop);
    tab1->add(timeSigRuler);

    auto* tempoRuler = new MarkerRuler(0, tabBarH + markerRulerH, winW, markerRulerH,
        80, 60, MarkerRuler::TEMPO, timeline, tPop);
    tab1->add(tempoRuler);

    auto* og2 = new SongEditor(0, tabBarH + 2*markerRulerH, winW,
                               10, 80, 45, 60, 0.25, *p2);
    tab1->add(og2);
    tab1->resizable(og2);
    tabs->add(*tab1);

    // ---- Loop Editor tab ----
    auto* tabLoop = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Loop Editor");
    tabLoop->end();
    tabLoop->color(bgColor);

    loopEd = new LoopEditor(0, tabBarH, winW, tabsH - tabBarH);
    tabLoop->add(loopEd);
    tabLoop->resizable(loopEd);
    tabs->add(*tabLoop);

    // ---- Pattern Editor tab ----
    auto* tab2 = new Fl_Group(0, tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab2->end();
    tab2->color(bgColor);
    patternTab = tab2;

    patternEd = new PatternEditor(0, tabBarH, winW, numRows,
                                  numPatternBeats, rowHeight, 40, 0.25, *p1);
    tab2->add(patternEd);

    drumEd = new DrumPatternEditor(0, tabBarH, winW, drumNumRows,
                                   numPatternBeats, drumRowH, 40, 0.25f, *p1);
    tab2->add(drumEd);
    drumEd->hide();

    patternPanel = new PatternPanel(0, tabsH - panelH, winW, panelH);
    patternPanel->setTimeline(timeline);
    tab2->add(patternPanel);
    tab2->resizable(patternEd);
    tabs->add(*tab2);

    // ---- Transport bar ----
    Fl_Group::current(nullptr);
    bottomPane = new Transport(0, tabsH, winW, bottomH, transport, timeline);
    window->add(bottomPane);
    if (disableTransportButtons)
        bottomPane->disableButtons();

    // ---- Wire up song editor ----
    og2->setTransport(transport, timeline);
    og2->onRulerOffsetChanged = [timeSigRuler, tempoRuler](int off, int clipLeft) {
        timeSigRuler->setOffsetX(off);
        timeSigRuler->setClipLeft(clipLeft);
        tempoRuler->setOffsetX(off);
        tempoRuler->setClipLeft(clipLeft);
    };
    og2->setContextPopup(ctxPop);
    og2->onEndReached = [this]() { bottomPane->notifyEndReached(); };
    og2->onSeek = [this]() {
        bottomPane->notifySeek();
        if (onExtraSeek) onExtraSeek();
    };
    og2->onPatternDoubleClick = [this, timeline, tab2](int trackIndex) {
        timeline->selectTrack(trackIndex);
        tabs->value(tab2);
        tabs->redraw();
    };
    if (verbose) {
        og2->setVerbose(true);
        if (getPitchName)
            og2->setPitchName(getPitchName);
    }

    // ---- Wire up loop editor ----
    loopEd->setTimeline(timeline);
    loopEd->setTransport(transport);
    loopEd->setContextPopup(ctxPop);

    // ---- Wire up pattern editors ----
    patternEd->setPatternPlayhead(transport, timeline, 0);
    drumEd->setPatternPlayhead(transport, timeline, 0);

    // ---- Note label / params sync ----
    auto syncNoteLabels = [this]() {
        patternEd->setNoteParams(patternPanel->rootPitch(),
                                 patternPanel->chordType(),
                                 patternPanel->isSharp());
        if (onExtraParamsChanged) onExtraParamsChanged();
    };
    patternPanel->onParamsChanged = syncNoteLabels;
    syncNoteLabels();

    // ---- Popups — added last (FLTK dispatches in reverse order) ----
    window->add(p1);     window->registerPopup(p1);
    window->add(p2);     window->registerPopup(p2);
    window->add(tPop);   window->registerPopup(tPop);
    window->add(tsPop);  window->registerPopup(tsPop);
    window->add(ctxPop); window->registerPopup(ctxPop);

    // ---- Resizable chain + minimum size ----
    window->resizable(tabs);
    const int minW = 14 + 36 + 5*40;
    const int minH = tabBarH + Editor::rulerH + 5*rowHeight + Editor::hScrollH + panelH + bottomH;
    window->size_range(minW, minH);

    // ---- Timeline observers ----
    timeline->addObserver(&editorSwitcher_);
    if (onExtraTimelineChange)
        timeline->addObserver(&changeNotifier_);

    // ---- Initial state ----
    timeline->selectTrack(0);
}

LuvieApp::~LuvieApp() {
    if (timeline_) {
        timeline_->removeObserver(&editorSwitcher_);
        timeline_->removeObserver(&changeNotifier_);
    }
}
