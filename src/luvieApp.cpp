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
#include "transport.hpp"
#include "markerPopup.hpp"
#include "markerRuler.hpp"
#include "patternPanel.hpp"
#include "trackContextPopup.hpp"
#include "paramLaneContextPopup.hpp"
#include "drumPatternEditor.hpp"
#include "pianorollEditor.hpp"
#include "loopEditor.hpp"
#include "outputsOverlay.hpp"
#include "transportOverlay.hpp"
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

void LuvieApp::saveCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);
    if (app->onSave) app->onSave();
}

void LuvieApp::saveAsCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);
    if (app->onSaveAs) app->onSaveAs();
}

void LuvieApp::importCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);

    Fl_Native_File_Chooser fc;
    fc.title("Import Session");
    fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    fc.filter("JSON Files\t*.json\nAll Files\t*");
    if (!lastFileDir.empty()) fc.directory(lastFileDir.c_str());
    if (fc.show() != 0) return;

    const char* path = fc.filename();
    if (!path || !path[0]) return;
    lastFileDir = std::filesystem::path(path).parent_path().string();

    AppState state;
    if (!loadAppState(path, state)) return;

    app->song_->loadTimeline(state.timeline);
    if (app->patternPanel)
        app->patternPanel->setParams(state.rootPitch, state.chordType, state.sharp);
}

void LuvieApp::exportCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);

    Fl_Native_File_Chooser fc;
    fc.title("Export Session");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter("JSON Files\t*.json\nAll Files\t*");
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if (!lastFileDir.empty()) fc.directory(lastFileDir.c_str());
    if (fc.show() != 0) return;

    std::string path = fc.filename();
    if (path.empty()) return;
    if (path.size() < 5 || path.substr(path.size() - 5) != ".json")
        path += ".json";
    lastFileDir = std::filesystem::path(path).parent_path().string();

    AppState state;
    state.timeline = app->song_->get();
    if (app->patternPanel) {
        state.rootPitch = app->patternPanel->rootPitch();
        state.chordType = app->patternPanel->chordType();
        state.sharp     = app->patternPanel->isSharp();
    }
    saveAppState(state, path);
}

void LuvieApp::build(AppWindow* window, ObservableSong* song, ObservablePattern* pattern,
                     ObservableInstrument* instruments, ITransport* transport) {
    song_         = song;
    pattern_      = pattern;
    instruments_  = instruments;

    const int off        = menuBarH;
    const int tabsH      = defaultWinH() - bottomH - menuBarH;
    const int drumRowH   = 20;
    const int numRows     = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / rowHeight;
    const int drumNumRows = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / drumRowH;

    Fl_Group::current(nullptr);

    // ---- Menu bar ----
    menuBar = new Fl_Menu_Bar(0, 0, winW, menuBarH);
    menuBar->box(FL_FLAT_BOX);
    menuBar->color(0xF3F4F600);
    menuBar->textcolor(0x37415100);
    menuBar->textsize(13);
    menuBar->add("File/Save",    FL_COMMAND + 's', nullptr, nullptr, 0);
    menuBar->add("File/Save As", 0,                nullptr, nullptr, FL_MENU_DIVIDER);
    menuBar->add("File/Import",  0,                nullptr, nullptr);
    menuBar->add("File/Export",  0,                nullptr, nullptr, 0);
    menuBar->add("View/Transport", 0, nullptr, nullptr, FL_MENU_TOGGLE);
    menuBar->add("View/Outputs", 0, nullptr, nullptr, FL_MENU_TOGGLE);
    window->add(menuBar);

    // ---- Popups (created before any group so they stay unparented until explicit add) ----
    auto* p1      = new NoteContextPopup{};
    auto* p2      = new NoteContextPopup{};
    auto* sp      = new PatternInstanceContextPopup{};
    auto* tPop    = new MarkerPopup(MarkerPopup::TEMPO);
    auto* tsPop   = new MarkerPopup(MarkerPopup::TIME_SIG);
    auto* ctxPop    = new TrackContextPopup;
    auto* plcPop    = new ParamLaneContextPopup;
    auto* pdPop      = new ParamDotPopup{};
    auto* nlCtxPop   = new NoteLabelsContextPopup;

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

    // ---- Song Editor tab ----
    auto* tab1 = new Fl_Group(0, off + tabBarH, winW, tabsH - tabBarH, "Song Editor");
    tab1->end();
    tab1->color(bgColor);

    auto* timeSigRuler = new MarkerRuler(0, off + tabBarH, winW, markerRulerH,
        60, 60, MarkerRuler::TIME_SIG, song, tsPop);
    tab1->add(timeSigRuler);

    auto* tempoRuler = new MarkerRuler(0, off + tabBarH + markerRulerH, winW, markerRulerH,
        60, 60, MarkerRuler::TEMPO, song, tPop);
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
        if (!outputsOverlay) return;
        outputsOverlay->show();
        if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("View/Outputs")))
            item->set();
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

    // ---- Wire up active pattern set ----
    loopEd->setActivePatterns(&aps);
    og2->setPlayheadActivePatterns(&aps);
    patternEd->setPlayheadActivePatterns(&aps);
    drumEd->setPlayheadActivePatterns(&aps);
    pianorollEd->setPlayheadActivePatterns(&aps);

    // ---- Wire up loop editor ----
    loopEd->setTimeline(song);
    loopEd->setPattern(pattern);
    loopEd->setTransport(transport);
    loopEd->setContextPopup(ctxPop);

    tabs->onModeChanged = [og2, transport, this](bool isLoop) {
        aps.clear();
        og2->setPlayheadLoopMode(isLoop);
        patternEd->setPlayheadLoopMode(isLoop);
        drumEd->setPlayheadLoopMode(isLoop);
        pianorollEd->setPlayheadLoopMode(isLoop);
        transport->setLoopMode(isLoop);
    };

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

    // ---- Note label / params sync ----
    auto syncNoteLabels = [this]() {
        patternEd->setNoteParams(patternPanel->rootPitch(),
                                 patternPanel->chordType(),
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

    // ---- Menu callbacks ----
    if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("File/Save")))
        item->callback(saveCb, this);
    if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("File/Save As")))
        item->callback(saveAsCb, this);
    if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("File/Import")))
        item->callback(importCb, this);
    if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("File/Export")))
        item->callback(exportCb, this);
    if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("View/Transport")))
        item->callback(transportCb, this);
    if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("View/Outputs")))
        item->callback(outputsCb, this);

    // ---- Popups — added last (FLTK dispatches in reverse order) ----
    window->add(p1);     window->registerPopup(p1);
    window->add(p2);     window->registerPopup(p2);
    window->add(sp);     window->registerPopup(sp);
    window->add(tPop);   window->registerPopup(tPop);
    window->add(tsPop);  window->registerPopup(tsPop);
    window->add(ctxPop); window->registerPopup(ctxPop);
    window->add(ctxPop->paramSubmenu); window->registerPopup(ctxPop->paramSubmenu);
    window->add(plcPop); window->registerPopup(plcPop);
    window->add(plcPop->paramSubmenu); window->registerPopup(plcPop->paramSubmenu);
    window->add(pdPop);  window->registerPopup(pdPop);
    window->add(nlCtxPop); window->registerPopup(nlCtxPop);
    window->add(nlCtxPop->paramSubmenu); window->registerPopup(nlCtxPop->paramSubmenu);
    // Hover popup: a positioned sub-window, but NOT registered — registering
    // would route mouse-moves through AppWindow's click-away logic and break the
    // indicator's enter/leave tracking.
    window->add(bottomPane->alertPopup());

    // Connections overlay last — large sub-window, click-away via registerPopup.
    {
        const int oy = menuBarH + 20;
        const int om = 20;
        outputsOverlay = new OutputsOverlay(om, oy, winW - 2*om, window->h() - oy - om);
        outputsOverlay->onClose = [this]() {
            if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("View/Outputs")))
                item->clear();
        };
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
        const int oy = menuBarH + 20;
        const int om = 20;
        transportOverlay = new TransportOverlay(om, oy, winW - 2*om, window->h() - oy - om, pluginMode);
        transportOverlay->onClose = [this]() {
            if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("View/Transport")))
                item->clear();
        };
        window->add(transportOverlay);
        window->registerPopup(transportOverlay);

        bottomPane->setIndicatorDoubleClick([this]() {
            if (!transportOverlay) return;
            transportOverlay->show();
            if (auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item("View/Transport")))
                item->set();
        });
    }

    // ---- Resizable chain + minimum size ----
    window->resizable(tabs);
    const int minW = 14 + 70 + 5*40;
    const int minH = menuBarH + tabBarH + Editor::rulerH + 5*rowHeight + Editor::hScrollH + panelH + bottomH;
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
        int id1 = instruments_->add("Instrument 1", false);
        int id2 = instruments_->add("Drums 1", true);
        outputsOverlay->setInstruments({
            {id1, "Instrument 1", port,  1, {}, false, false, -1, -1, -1, -1},
            {id2, "Drums 1",      port, 10, {}, true,  false, -1, -1, -1, -1}
        });
    }
    pushInstruments();
    song_->selectTrack(0);
}

void LuvieApp::disableSaveMenu(bool save, bool saveAs) {
    if (!menuBar) return;
    auto setActive = [this](const char* path, bool active) {
        auto* item = const_cast<Fl_Menu_Item*>(menuBar->find_item(path));
        if (!item) return;
        if (active) item->activate(); else item->deactivate();
    };
    setActive("File/Save",    !save);
    setActive("File/Save As", !saveAs);
    menuBar->redraw();
}

void LuvieApp::outputsCb(Fl_Widget* w, void* data) {
    auto* app  = static_cast<LuvieApp*>(data);
    auto* item = static_cast<Fl_Menu_Item*>(
        const_cast<Fl_Menu_Item*>(app->menuBar->find_item("View/Outputs")));
    if (!item || !app->outputsOverlay) return;
    if (item->value())
        app->outputsOverlay->show();
    else
        app->outputsOverlay->hide();
}

void LuvieApp::transportCb(Fl_Widget* w, void* data) {
    auto* app  = static_cast<LuvieApp*>(data);
    auto* item = static_cast<Fl_Menu_Item*>(
        const_cast<Fl_Menu_Item*>(app->menuBar->find_item("View/Transport")));
    if (!item || !app->transportOverlay) return;
    if (item->value())
        app->transportOverlay->show();
    else
        app->transportOverlay->hide();
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
                    ? pattern_->createDrumPattern(numPatternBeats)
                    : pattern_->createPattern(numPatternBeats);
                pattern_->setPatternInstrument(patId, ci.id);
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
