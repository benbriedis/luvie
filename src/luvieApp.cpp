#include "luvieApp.hpp"
#include "FL/Fl_Menu_Item.H"
#include "FL/Fl_Native_File_Chooser.H"
#include "timelineIO.hpp"
#include "observableSong.hpp"
#include "observablePattern.hpp"
#include "observableInstrument.hpp"
#include <filesystem>
#include "appWindow.hpp"
#include "songEditor.hpp"
#include "patternEditor.hpp"
#include "noteContextPopup.hpp"
#include "patternInstanceContextPopup.hpp"
#include "modernTabs.hpp"
#include "settingsButton.hpp"
#include "settingsMenuPopup.hpp"
#include "transport.hpp"
#include "markerPopup.hpp"
#include "markerRuler.hpp"
#include "patternPanel.hpp"
#include "trackContextPopup.hpp"
#include "loopContextPopup.hpp"
#include "paramLaneContextPopup.hpp"
#include "drumPatternEditor.hpp"
#include "pianorollEditor.hpp"
#include "loopEditor.hpp"
#include "outputsOverlay.hpp"
#include "transportOverlay.hpp"
#include "startupOverlay.hpp"
#include "paramDotPopup.hpp"
#include "noteLabelsContextPopup.hpp"
#include "patternParamGrid.hpp"

std::string LuvieApp::lastFileDir;

void LuvieApp::ChangeNotifier::onTimelineChanged() {
    if (app->outputsOverlay) app->outputsOverlay->refreshInstrumentButtons();
    if (app->onExtraTimelineChange) app->onExtraTimelineChange();
}

void LuvieApp::EditorSwitcher::onTimelineChanged() {
    if (!app->patternEd || !app->drumEd || !app->pianorollEd || !app->song_) return;
    const auto& data = app->song_->get();
    PatternType type = PatternType::STANDARD;
    {
        int patId = data.patternIdForSelectedLane();
        for (const auto& p : data.patterns)
            if (p.id == patId) { type = p.type; break; }
    }
    if (type == PatternType::DRUM) {
        app->patternEd->hide(); app->pianorollEd->hide(); app->drumEd->show();
    } else if (type == PatternType::PIANOROLL) {
        app->patternEd->hide(); app->drumEd->hide(); app->pianorollEd->show();
    } else {
        app->drumEd->hide(); app->pianorollEd->hide(); app->patternEd->show();
    }
}

void LuvieApp::saveAsCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);
    if (app->onSaveAs) app->onSaveAs();
}

void LuvieApp::importCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);

    Fl_Native_File_Chooser fc;
    fc.title("Import Project");
    fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    fc.filter("Luvie Projects\t*.luv\nAll Files\t*");
    if (!lastFileDir.empty()) fc.directory(lastFileDir.c_str());
    if (fc.show() != 0) return;

    const char* path = fc.filename();
    if (!path || !path[0]) return;
    lastFileDir = std::filesystem::path(path).parent_path().string();

    AppState state;
    if (!loadAppState(path, state)) return;

    app->song_->loadTimeline(state.timeline);
    if (app->onApplyOutputs) app->onApplyOutputs(state);
}

void LuvieApp::exportCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);

    Fl_Native_File_Chooser fc;
    fc.title("Export Project");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter("Luvie Projects\t*.luv\nAll Files\t*");
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if (!lastFileDir.empty()) fc.directory(lastFileDir.c_str());
    if (fc.show() != 0) return;

    std::string path = fc.filename();
    if (path.empty()) return;
    if (path.size() < 4 || path.substr(path.size() - 4) != ".luv")
        path += ".luv";
    lastFileDir = std::filesystem::path(path).parent_path().string();

    AppState state;
    state.timeline = app->song_->get();
    if (app->onCollectOutputs) app->onCollectOutputs(state);
    saveAppState(state, path);
}

void LuvieApp::build(AppWindow* window, ObservableSong* song, ObservablePattern* pattern,
                     ObservableInstrument* instruments, ITransport* transport) {
    song_         = song;
    pattern_      = pattern;
    instruments_  = instruments;

    const int off        = 0;
    const int tabsH      = defaultWinH() - bottomH;
    const int drumRowH   = 20;
    const int numRows     = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / rowHeight;
    const int drumNumRows = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / drumRowH;

    Fl_Group::current(nullptr);

    // ---- Popups (created before any group so they stay unparented until explicit add) ----
    auto* p1      = new NoteContextPopup{};
    auto* p2      = new NoteContextPopup{};
    auto* sp      = new PatternInstanceContextPopup{};
    auto* tPop    = new MarkerPopup(MarkerPopup::TEMPO);
    auto* tsPop   = new MarkerPopup(MarkerPopup::TIME_SIG);
    auto* ctxPop    = new TrackContextPopup;
    auto* loopCtxPop = new LoopContextPopup;
    auto* plcPop    = new ParamLaneContextPopup;
    auto* pdPop      = new ParamDotPopup{};
    auto* nlCtxPop   = new NoteLabelsContextPopup;
    auto* settingsPop = new SettingsMenuPopup;

    // ---- Tabs ----
    static constexpr Fl_Color songColor = 0x22C55E00;
    static constexpr Fl_Color loopColor = 0x3B82F600;

    tabs = new ModernTabs(0, off, winW, tabsH);
    tabs->end();
    tabs->enableModeToggle(songColor, loopColor);
    tabs->setTabAccent(0, songColor);
    tabs->setTabAccent(1, loopColor);
    tabs->setTabAccent(2, 0xF9731600);
    window->add(tabs);

    // ---- Settings gear (right end of the tab bar) ----
    // Square, tab-bar height; drops a menu carrying the former File/View items.
    constexpr int gearSize = tabBarH;
    settingsButton = new SettingsButton(winW - gearSize, off, gearSize, gearSize);
    settingsButton->onClick = [this, settingsPop] {
        // Drop the menu from below the gear, right-aligned so it stays inside
        // the window rather than spilling past its right edge.
        int mx = settingsButton->x() + settingsButton->w() - SettingsMenuPopup::popW;
        settingsPop->open(std::max(0, mx), settingsButton->y() + settingsButton->h());
    };
    window->add(settingsButton);
    tabs->setRightWidget(settingsButton, gearSize);

    settingsMenu = settingsPop;
    settingsPop->onSaveAs    = [this] { saveAsCb   (nullptr, this); };
    settingsPop->onImport    = [this] { importCb   (nullptr, this); };
    settingsPop->onExport    = [this] { exportCb   (nullptr, this); };
    settingsPop->onTransport = [this] { transportCb(nullptr, this); };
    settingsPop->onOutputs   = [this] { outputsCb  (nullptr, this); };

    // ---- Song Editor tab ----
    auto* tab1 = new Fl_Group(0, off + tabBarH, winW, tabsH - tabBarH, "Song Editor");
    tab1->end();
    tab1->color(bgColor);

    auto* timeSigRuler = new MarkerRuler(0, off + tabBarH, winW, markerRulerH,
        60, 60, MarkerRuler::TIME_SIG, song, tPop, tsPop);
    tab1->add(timeSigRuler);

    auto* tempoRuler = new MarkerRuler(0, off + tabBarH + markerRulerH, winW, markerRulerH,
        60, 60, MarkerRuler::TEMPO, song, tPop, tsPop);
    tab1->add(tempoRuler);

    auto* og2 = new SongEditor(0, off + tabBarH + 2*markerRulerH, winW,
                               10, 60, 45, 60, 0.25, *p2);
    songEd = og2;
    tab1->add(og2);
    tab1->resizable(og2);
    tabs->add(*tab1);

    // ---- Loop Editor tab ----
    auto* tabLoop = new Fl_Group(0, off + tabBarH, winW, tabsH - tabBarH, "Loop Editor");
    tabLoop->end();
    tabLoop->color(bgColor);

    loopEd = new LoopEditor(0, off + tabBarH, winW, tabsH - tabBarH);
    tabLoop->add(loopEd);
    tabLoop->resizable(loopEd);
    tabs->add(*tabLoop);

    // ---- Pattern Editor tab ----
    auto* tab2 = new Fl_Group(0, off + tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab2->end();
    tab2->color(bgColor);
    patternTab = tab2;

    patternEd = new PatternEditor(0, off + tabBarH, winW, numRows,
                                  numPatternBeats, rowHeight, 40, 0.25, *p1);
    tab2->add(patternEd);

    drumEd = new DrumPatternEditor(0, off + tabBarH, winW, drumNumRows,
                                   numPatternBeats, drumRowH, 40, 0.25f, *p1);
    tab2->add(drumEd);
    drumEd->hide();

    pianorollEd = new PianorollEditor(0, off + tabBarH, winW, drumNumRows,
                                      numPatternBeats, drumRowH, 40, 0.25f, *p1);
    tab2->add(pianorollEd);
    pianorollEd->hide();

    // Stretch the editors down to the control panel so their vertical
    // scrollbars meet it (this resize also forces a relayout that fills them).
    const int patEditorH = tabsH - tabBarH - panelH;
    patternEd->resize(0, off + tabBarH, winW, patEditorH);
    drumEd->resize(0, off + tabBarH, winW, patEditorH);
    pianorollEd->resize(0, off + tabBarH, winW, patEditorH);

    patternPanel = new PatternPanel(0, off + tabsH - panelH, winW, panelH);
    patternPanel->setPattern(pattern);
    tab2->add(patternPanel);
    tab2->resizable(patternEd);
    tabs->add(*tab2);

    // ---- Transport bar ----
    Fl_Group::current(nullptr);
    bottomPane = new Transport(0, off + tabsH, winW, bottomH, transport);
    window->add(bottomPane);
    if (disableTransportButtons)
        bottomPane->disableButtons();

    // ---- Wire up song editor ----
    og2->setTransport(transport, song);
    og2->onRulerOffsetChanged = [timeSigRuler, tempoRuler](int off, int clipLeft) {
        timeSigRuler->setOffsetX(off);
        timeSigRuler->setClipLeft(clipLeft);
        tempoRuler->setOffsetX(off);
        tempoRuler->setClipLeft(clipLeft);
    };
    og2->onNumColsChanged = [timeSigRuler, tempoRuler](int n) {
        timeSigRuler->setNumCols(n);
        tempoRuler->setNumCols(n);
    };
    og2->setPattern(pattern);
    og2->setContextPopup(ctxPop);
    ctxPop->onShowInstruments = [this]() {
        if (outputsOverlay) outputsOverlay->show();
    };
    og2->setParamLaneContextPopup(plcPop);
    og2->onEndReached = [this]() { bottomPane->notifyEndReached(); };
    og2->onSeek = [this]() {
        bottomPane->notifySeek();
        if (onExtraSeek) onExtraSeek();
    };
    auto openPatternTab = [this, song, tab2](int trackIndex, int laneId) {
        song->selectLane(trackIndex, laneId);
        tabs->value(tab2);
        tabs->redraw();
    };
    og2->onPatternDoubleClick = openPatternTab;
    og2->onOpenPattern        = openPatternTab;
    og2->setSongPopup(sp);
    og2->setParamDotPopup(pdPop);
    ctxPop->onOpenPattern     = openPatternTab;
    if (verbose) {
        og2->setVerbose(true);
        if (getPitchName)
            og2->setPitchName(getPitchName);
    }
    // Soft (Native/Debug) MIDI output: drive non-Jack ports from the song playhead.
    og2->setPlayheadPortRegistry(portRegistry);
    if (rowToMidi || instrRoute)
        og2->setPlayheadSoftRouting(rowToMidi, instrRoute);

    // Clicking a pattern-editor row label auditions that note on the selected
    // track's instrument port.
    auditioner.setPortRegistry(portRegistry);
    auditioner.setInstrRoute(instrRoute);

    // ---- Wire up active pattern set ----
    loopEd->setLoopManager(&loopMgr);
    og2->setPlayheadLoopManager(&loopMgr);
    patternEd->setPlayheadLoopManager(&loopMgr);
    drumEd->setPlayheadLoopManager(&loopMgr);
    pianorollEd->setPlayheadLoopManager(&loopMgr);

    // ---- Wire up loop editor ----
    loopEd->setTimeline(song);
    loopEd->setPattern(pattern);
    loopEd->setTransport(transport);
    loopEd->setContextPopup(loopCtxPop);
    loopCtxPop->onOpenPattern     = openPatternTab;
    loopCtxPop->onShowInstruments = [this]() {
        if (outputsOverlay) outputsOverlay->show();
    };

    // The mode toggle keeps the currently-sounding loops alive across a switch: it
    // no longer clears loopMgr. The controller freezes the song playhead on
    // Song→Loop and does the bar-aligned seek-back on Loop→Song.
    modeController.init(transport, tabs, og2, [this](bool loop) {
        songEd->setPlayheadLoopMode(loop);
        patternEd->setPlayheadLoopMode(loop);
        drumEd->setPlayheadLoopMode(loop);
        pianorollEd->setPlayheadLoopMode(loop);
    });
    tabs->onModeChanged = [this](bool isLoop) { modeController.requestMode(isLoop); };

    // ---- Wire up pattern editors ----
    patternEd->setPatternPlayhead(transport, pattern, 0);
    drumEd->setPatternPlayhead(transport, pattern, 0);
    pianorollEd->setPatternPlayhead(transport, pattern, 0);
    patternEd->setNoteLabelsContextPopup(nlCtxPop);
    drumEd->setNoteLabelsContextPopup(nlCtxPop);
    pianorollEd->setNoteLabelsContextPopup(nlCtxPop);
    patternEd->setParamLabelsContextPopup(nlCtxPop);
    drumEd->setParamLabelsContextPopup(nlCtxPop);
    pianorollEd->setParamLabelsContextPopup(nlCtxPop);
    patternEd->setParamDotPopup(pdPop);
    drumEd->setParamDotPopup(pdPop);
    pianorollEd->setParamDotPopup(pdPop);
    patternEd->setAuditioner(&auditioner);
    drumEd->setAuditioner(&auditioner);
    pianorollEd->setAuditioner(&auditioner);

    // ---- Note label / params sync ----
    auto syncNoteLabels = [this]() {
        patternEd->setNoteParams(patternPanel->rootPitch(),
                                 patternPanel->chordHash(),
                                 patternPanel->isSharp());
        if (onExtraParamsChanged) onExtraParamsChanged();
    };
    patternPanel->onParamsChanged = syncNoteLabels;
    patternPanel->onSnapChanged = [this](float s) {
        if (patternEd)   patternEd->setSnap(s);
        if (drumEd)      drumEd->setSnap(s);
        if (pianorollEd) pianorollEd->setSnap(s);
    };
    patternPanel->onFocus = [this]() {
        if (drumEd && drumEd->visible())
            drumEd->focusPattern();
        else if (pianorollEd && pianorollEd->visible())
            pianorollEd->focusPattern();
        else if (patternEd)
            patternEd->focusPattern();
    };
    patternPanel->onRapidChanged = [this](bool r) {
        if (patternEd) patternEd->setRapidMode(r);
    };
    syncNoteLabels();

    // ---- Popups — added last (FLTK dispatches in reverse order) ----
    window->add(p1);     window->registerPopup(p1);
    window->add(p2);     window->registerPopup(p2);
    window->add(sp);     window->registerPopup(sp);
    window->add(tPop);   window->registerPopup(tPop);
    window->add(tsPop);  window->registerPopup(tsPop);
    window->add(ctxPop); window->registerPopup(ctxPop);
    window->add(ctxPop->paramSubmenu); window->registerPopup(ctxPop->paramSubmenu);
    window->add(loopCtxPop); window->registerPopup(loopCtxPop);
    window->add(plcPop); window->registerPopup(plcPop);
    window->add(plcPop->paramSubmenu); window->registerPopup(plcPop->paramSubmenu);
    window->add(pdPop);  window->registerPopup(pdPop);
    window->add(nlCtxPop); window->registerPopup(nlCtxPop);
    window->add(nlCtxPop->paramSubmenu); window->registerPopup(nlCtxPop->paramSubmenu);
    window->add(settingsPop); window->registerPopup(settingsPop);
    // Hover popup: a positioned sub-window, but NOT registered — registering
    // would route mouse-moves through AppWindow's click-away logic and break the
    // indicator's enter/leave tracking.
    window->add(bottomPane->alertPopup());

    // Connections overlay last — large sub-window, click-away via registerPopup.
    {
        const int oy = 20;
        const int om = 20;
        outputsOverlay = new OutputsOverlay(om, oy, winW - 2*om, window->h() - oy - om);
        outputsOverlay->isInstrumentInUse = [this](int instrId) {
            for (const auto& p : song_->get().patterns)
                if (p.instrumentId == instrId) return true;
            return false;
        };
        outputsOverlay->onInstrumentsChanged = [this]() {
            pushInstruments();
            if (onInstrumentsChanged) onInstrumentsChanged();
        };
        outputsOverlay->setObservableInstrument(instruments_);
        if (drumEd) {
            drumEd->onDrumLabelChanged = [this](int instrId, int midiNote, const std::string& label) {
                outputsOverlay->updateInstrumentDrumMap(instrId, midiNote, label);
            };
        }
        window->add(outputsOverlay);
        window->registerPopup(outputsOverlay);
    }

    // Transport overlay — same footprint as the outputs overlay.
    {
        const int oy = 20;
        const int om = 20;
        transportOverlay = new TransportOverlay(om, oy, winW - 2*om, window->h() - oy - om, pluginMode);
        window->add(transportOverlay);
        window->registerPopup(transportOverlay);
    }

    // New-project startup dialog — centred over the main window. main() shows it
    // (and wires its callbacks) only for a fresh project. Deliberately NOT
    // registered as a popup: it must be dismissed via its Confirm button, not by
    // clicking away.
    {
        const int dlgW = 380;
        const int dlgH = 250;
        const int dx = (winW - dlgW) / 2;
        const int dy = (window->h() - dlgH) / 2;
        startupOverlay = new StartupOverlay(dx, dy, dlgW, dlgH, pluginMode);
        window->add(startupOverlay);
    }

    // ---- Resizable chain + minimum size ----
    window->resizable(tabs);
    const int minW = 14 + 70 + 5*40;
    const int minH = tabBarH + Editor::rulerH + 5*rowHeight + Editor::hScrollH + panelH + bottomH;
    window->size_range(minW, minH);

    // ---- Timeline observers ----
    song_->addObserver(&editorSwitcher_);
    song_->addObserver(&changeNotifier_);

    // Wire pattern panel to instrument observable
    if (patternPanel)
        patternPanel->setInstruments(instruments_);

    // ---- Initial state ----
    // Seed with default instruments on a fresh (empty) session.
    if (song_->get().instruments.empty()) {
        const std::string port = outputsOverlay->getOutputs().empty()
            ? "" : outputsOverlay->getOutputs()[0];
        int id1 = instruments_->add("Instrument A", false);
        int id2 = instruments_->add("Drums A", true);
        outputsOverlay->setInstruments({
            {id1, "Instrument A", port,  1, {}, false, false, -1, -1, -1, -1},
            {id2, "Drums A",      port, 10, {}, true,  false, -1, -1, -1, -1}
        });
    }
    pushInstruments();
    song_->selectTrack(0);
}

void LuvieApp::disableSaveMenu(bool saveAs) {
    if (!settingsMenu) return;
    settingsMenu->setSaveAsEnabled(!saveAs);
}

void LuvieApp::outputsCb(Fl_Widget* w, void* data) {
    auto* app = static_cast<LuvieApp*>(data);
    if (app->outputsOverlay) app->outputsOverlay->show();
}

void LuvieApp::transportCb(Fl_Widget* w, void* data) {
    auto* app = static_cast<LuvieApp*>(data);
    if (app->transportOverlay) app->transportOverlay->show();
}

void LuvieApp::pushInstruments() {
    if (!outputsOverlay || !song_) return;
    const auto& instrs = outputsOverlay->getInstruments();

    // Update default instrument IDs for newly created patterns
    song_->defaultInstrumentId     = 0;
    song_->defaultDrumInstrumentId = 0;
    for (const auto& ci : instrs) {
        if (!ci.isDrum && !song_->defaultInstrumentId)
            song_->defaultInstrumentId = ci.id;
        if (ci.isDrum && !song_->defaultDrumInstrumentId)
            song_->defaultDrumInstrumentId = ci.id;
    }

    // Sync tracks 1:1 with instruments (only when overlay has instruments)
    if (!instrs.empty()) {
        // Remove tracks whose instrumentId no longer matches any overlay instrument
        std::vector<int> toRemove;
        for (const auto& t : song_->get().tracks) {
            bool found = false;
            for (const auto& ci : instrs)
                if (ci.id == t.instrumentId) { found = true; break; }
            if (!found) toRemove.push_back(t.id);
        }
        for (int id : toRemove)
            song_->removeTrackAndPattern(id);

        // Add a track for each instrument that doesn't have one yet
        for (const auto& ci : instrs) {
            bool found = false;
            for (const auto& t : song_->get().tracks)
                if (t.instrumentId == ci.id) { found = true; break; }
            if (!found && pattern_) {
                int patId = ci.isDrum
                    ? pattern_->createDrumPattern(numPatternBeats, ci.id)
                    : pattern_->createPattern(numPatternBeats, ci.id);
                song_->addTrack(ci.id, patId);
            }
        }
    }

    if (drumEd) {
        std::map<int, std::map<int, std::string>> allMaps;
        std::map<int, bool> allFallbacks;
        for (const auto& ci : instrs) {
            allMaps[ci.id]      = ci.drumMap;
            allFallbacks[ci.id] = ci.fallbackNoteNames;
        }
        drumEd->setAllDrumMaps(allMaps, allFallbacks);
    }
    outputsOverlay->refreshInstrumentButtons();
}

LuvieApp::~LuvieApp() {
    if (song_) {
        song_->removeObserver(&editorSwitcher_);
        song_->removeObserver(&changeNotifier_);
    }
}
