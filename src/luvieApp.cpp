// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "luvieApp.hpp"
#include "luvieDebug.hpp"
#include "FL/Fl_Menu_Item.H"
#include "FL/Fl_Native_File_Chooser.H"
#include "timelineIO.hpp"
#include "observableSong.hpp"
#include "observablePattern.hpp"
#include "observableInstrument.hpp"
#include <algorithm>
#include <filesystem>
#include "appWindow.hpp"
#include "songEditor.hpp"
#include "harmonyEditor.hpp"
#include "noteContextPopup.hpp"
#include "patternInstanceContextPopup.hpp"
#include "pasteContextPopup.hpp"
#include "selectionContextPopup.hpp"
#include "modernTabs.hpp"
#include "settingsButton.hpp"
#include "settingsMenuPopup.hpp"
#include "transport.hpp"
#include "markerPopup.hpp"
#include "markerRuler.hpp"
#include "loopRuler.hpp"
#include "patternPanel.hpp"
#include "songPanel.hpp"
#include "trackContextPopup.hpp"
#include "loopContextPopup.hpp"
#include "loopRulerContextPopup.hpp"
#include "paramLaneContextPopup.hpp"
#include "drumPatternEditor.hpp"
#include "pianorollEditor.hpp"
#include "chords.hpp"          // ccForType
#include "loopEditor.hpp"
#include "outputsOverlay.hpp"
#include "transportOverlay.hpp"
#include "startupOverlay.hpp"
#include "paramDotPopup.hpp"
#include "noteLabelsContextPopup.hpp"
#include "patternParamGrid.hpp"

// The pattern tab's control panel is bottom-anchored and can change height (it
// folds into two rows once the window is too narrow for one), so the editors
// above it have to be re-fitted after every resize rather than by the plain
// Fl_Group resizable() rule.
class PatternTabGroup : public Fl_Group {
public:
    std::function<void()> onLayout;
    PatternTabGroup(int x, int y, int w, int h, const char* l)
        : Fl_Group(x, y, w, h, l) {}
    void resize(int x, int y, int w, int h) override {
        Fl_Group::resize(x, y, w, h);
        if (onLayout) onLayout();
    }
};

std::string LuvieApp::lastFileDir;

void LuvieApp::layoutPatternTab()
{
    if (!patternTab || !patternPanel) return;
    // The panel's height request depends on its own width, which this very call
    // sets — the guard keeps the callback it fires from recursing.
    if (layingOutPatternTab) return;
    layingOutPatternTab = true;

    const int panelHeight = patternPanel->heightForWidth(patternTab->w());
    const int editorH     = std::max(0, patternTab->h() - panelHeight);
    for (Fl_Widget* e : { (Fl_Widget*)harmonyEd, (Fl_Widget*)drumEd, (Fl_Widget*)pianorollEd })
        if (e) e->resize(patternTab->x(), patternTab->y(), patternTab->w(), editorH);
    patternPanel->resize(patternTab->x(), patternTab->y() + editorH,
                         patternTab->w(), panelHeight);

    layingOutPatternTab = false;
}

void LuvieApp::ChangeNotifier::onTimelineChanged() {
    if (app->outputsOverlay) app->outputsOverlay->refreshInstrumentButtons();
    if (app->onExtraTimelineChange) app->onExtraTimelineChange();
}

void LuvieApp::EditorSwitcher::onTimelineChanged() {
    if (!app->harmonyEd || !app->drumEd || !app->pianorollEd || !app->song_) return;
    const auto& data = app->song_->get();
    PatternType type = PatternType::HARMONY;
    {
        int patId = data.patternIdForSelectedLane();
        for (const auto& p : data.patterns)
            if (p.id == patId) { type = p.type; break; }
    }
    if (type == PatternType::DRUM) {
        app->harmonyEd->hide(); app->pianorollEd->hide(); app->drumEd->show();
    } else if (type == PatternType::PIANOROLL) {
        app->harmonyEd->hide(); app->drumEd->hide(); app->pianorollEd->show();
    } else {
        app->drumEd->hide(); app->pianorollEd->hide(); app->harmonyEd->show();
    }
    // Runs on every timeline notify, but updateMidiTarget() returns immediately
    // unless the visible editor actually changed.
    app->updateMidiTarget();
}

// Which pattern editor is on screen, if any. Two things decide it: the selected
// tab, and — within the Pattern Editor tab — which of the three editors
// EditorSwitcher has shown for the selected pattern's type.
void LuvieApp::updateMidiTarget()
{
    BasePatternEditor* next = nullptr;
    if (tabs && patternTab && tabs->value() == patternTab) {
        if (pianorollEd && pianorollEd->visible())      next = pianorollEd;
        else if (drumEd && drumEd->visible())           next = drumEd;
        else if (harmonyEd && harmonyEd->visible())     next = harmonyEd;
    }
    if (next == midiTarget) return;

    // Leaving an editor ends its take and releases anything it was sounding, so a
    // key held while switching tabs neither hangs nor keeps recording.
    if (midiTarget) {
        midiTarget->setRecordArmed(false);
        midiTarget->setGrowArmed(false);
        midiTarget->releaseMidiNotes();
        midiTarget->clearLiveNotes();
    }
    if (patternPanel) patternPanel->stopRecording();
    midiTarget = next;
}

int LuvieApp::midiInInstrument() const
{
    if (midiTarget) return midiTarget->currentInstrumentId();
    if (!song_) return -1;
    const auto& tl  = song_->get();
    const int   sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) return -1;
    return tl.tracks[sel].instrumentId;
}

bool LuvieApp::midiInAccepted(int slot, uint8_t status) const
{
    if (!outputsOverlay) return true;
    std::string inputName;
    int         channel = 0;
    if (!outputsOverlay->instrumentInput(midiInInstrument(), inputName, channel))
        return true;
    if (midiIn.slotForName(inputName) != slot) return false;
    if (channel == 0) return true;                       // "Any"
    if (status < 0x80 || status >= 0xF0) return true;    // not a channel message
    return (status & 0x0F) == channel - 1;
}

void LuvieApp::stopMidiRecording()
{
    if (midiTarget) midiTarget->releaseMidiNotes();
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
    fc.filter("Luvie Projects\t*.luvie\nAll Files\t*");
    if (!lastFileDir.empty()) fc.directory(lastFileDir.c_str());
    if (fc.show() != 0) return;

    const char* path = fc.filename();
    if (!path || !path[0]) return;
    lastFileDir = std::filesystem::path(path).parent_path().string();

    AppState state;
    if (!loadAppState(path, state)) return;

    app->song_->resetGlobalTempo();   // a jam tempo is the session's, not this song's
    app->song_->loadTimeline(state.timeline);
    if (app->onApplyOutputs) app->onApplyOutputs(state);
    app->applyLoopState(state.loopMode, state.activeLoopPatterns);
    app->applyLoopTimeSig(state.loopSigTop, state.loopSigBottom, state.loopSigBeat);
    app->applyScenes(state.scenes, state.currentScene);
    app->midiLearn.setBindings(state.midiLearn);
}

void LuvieApp::exportCb(Fl_Widget*, void* data) {
    auto* app = static_cast<LuvieApp*>(data);

    Fl_Native_File_Chooser fc;
    fc.title("Export Project");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter("Luvie Projects\t*.luvie\nAll Files\t*");
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if (!lastFileDir.empty()) fc.directory(lastFileDir.c_str());
    if (fc.show() != 0) return;

    std::string path = fc.filename();
    if (path.empty()) return;
    if (path.size() < 6 || path.substr(path.size() - 6) != ".luvie")
        path += ".luvie";
    lastFileDir = std::filesystem::path(path).parent_path().string();

    AppState state;
    state.timeline = app->song_->get();
    if (app->onCollectOutputs) app->onCollectOutputs(state);
    state.loopMode           = app->isLoopMode();
    state.activeLoopPatterns = app->activeLoopPatterns();
    state.scenes             = app->sceneSets();
    state.currentScene       = app->shownScene();
    app->loopTimeSig(state.loopSigTop, state.loopSigBottom, state.loopSigBeat);
    state.midiLearn          = app->midiLearn.bindings();
    saveAppState(state, path);
}

std::array<ISelectionHost*, 4> LuvieApp::selectionHosts() const {
    return { songEd      ? songEd->selectionHost()      : nullptr,
             harmonyEd   ? harmonyEd->selectionHost()   : nullptr,
             pianorollEd ? pianorollEd->selectionHost() : nullptr,
             drumEd      ? drumEd->selectionHost()      : nullptr };
}

void LuvieApp::build(AppWindow* window, ObservableSong* song, ObservablePattern* pattern,
                     ObservableInstrument* instruments, ITransport* transport) {
    song_         = song;
    pattern_      = pattern;
    instruments_  = instruments;
    transport_    = transport;

    window->onUndo = [song]() { song->undo(); };
    window->onRedo = [song]() { song->redo(); };
    // Only one editor is visible at a time, so clearing all of them is both
    // correct and simpler than working out which one has the cursor.
    window->onEscape = [this]() {
        bool cleared = false;
        for (ISelectionHost* h : selectionHosts())
            if (h && h->hasSelection()) { h->clearSelection(); cleared = true; }
        return cleared;
    };

    // Ctrl-A goes to whichever editor is on screen, wherever the cursor sits.
    window->onSelectAll = [this]() {
        for (ISelectionHost* h : selectionHosts())
            if (h && h->showing()) h->selectAllItems();
    };

    // Delete acts on the selection wherever the cursor is; with nothing selected
    // it says so, and the grid under the cursor deletes the note it is hovering.
    window->onDeleteSelection = [this]() {
        for (ISelectionHost* h : selectionHosts())
            if (h && h->showing() && h->hasSelection()) { h->deleteSelectedItems(); return true; }
        return false;
    };

    // Ctrl-X acts on the selection wherever the cursor is, like Ctrl-C does.
    window->onCut = [this]() {
        for (ISelectionHost* h : selectionHosts())
            if (h && h->showing() && h->hasSelection()) { h->cutSelection(); return; }
    };

    // Ctrl-C acts on the selection wherever the cursor is, like Delete does.
    window->onCopy = [this]() {
        for (ISelectionHost* h : selectionHosts())
            if (h && h->showing() && h->hasSelection()) { h->copySelection(); return; }
    };

    // Ctrl-V, unlike the rest, does care where the cursor is: it is what chooses
    // where the copy lands. Over anything but an editing grid it does nothing.
    window->onPaste = [this]() {
        for (ISelectionHost* h : selectionHosts())
            if (h && h->showing() && h->ownsWindowPoint(Fl::event_x(), Fl::event_y()))
                { h->pasteClipboard(Fl::event_x(), Fl::event_y()); return; }
    };

    // A click anywhere but the grid holding the selection dismisses it. Clicks
    // inside that grid are left alone: it has its own rules for them (band
    // sweeps, ctrl-toggles, dragging the selection).
    window->onClick = [this](int wx, int wy) {
        for (ISelectionHost* h : selectionHosts()) {
            if (!h || h->ownsWindowPoint(wx, wy)) continue;
            if (h->hasSelection()) h->clearSelection();
        }
    };

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
    auto* selPop  = new SelectionContextPopup{};
    auto* pastePop = new PasteContextPopup{};
    auto* tPop    = new MarkerPopup(MarkerPopup::TEMPO);
    auto* tsPop   = new MarkerPopup(MarkerPopup::TIME_SIG);
    auto* ctxPop    = new TrackContextPopup;
    auto* loopCtxPop = new LoopContextPopup;
    auto* loopRulerPop = new LoopRulerContextPopup;
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

    auto* loopRuler = new LoopRuler(0, off + tabBarH + 2*markerRulerH, winW, markerRulerH,
        60, 60);
    loopRuler->setContextPopup(loopRulerPop);
    tab1->add(loopRuler);
    this->loopRuler = loopRuler;

    // The editor fills the tab bar-to-bar: everything under the three marker
    // rulers except the control bar anchored at the bottom.
    const int songBodyY = off + tabBarH + 3*markerRulerH;
    const int songBodyH = (tabsH - tabBarH) - 3*markerRulerH - panelH;
    auto* og2 = new SongEditor(0, songBodyY, winW, 10, 60, 45, 60, 0.25, *p2);
    og2->size(winW, songBodyH);
    songEd = og2;
    tab1->add(og2);

    songPanel = new SongPanel(0, songBodyY + songBodyH, winW, panelH);
    tab1->add(songPanel);

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
    auto* tab2 = new PatternTabGroup(0, off + tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab2->end();
    tab2->color(bgColor);
    patternTab = tab2;

    harmonyEd = new HarmonyEditor(0, off + tabBarH, winW, numRows,
                                  numPatternBeats, rowHeight, 40, 1.0f, *p1);
    tab2->add(harmonyEd);

    drumEd = new DrumPatternEditor(0, off + tabBarH, winW, drumNumRows,
                                   numPatternBeats, drumRowH, 40, 1.0f, *p1);
    tab2->add(drumEd);
    drumEd->hide();

    pianorollEd = new PianorollEditor(0, off + tabBarH, winW, drumNumRows,
                                      numPatternBeats, drumRowH, 40, 1.0f, *p1);
    tab2->add(pianorollEd);
    pianorollEd->hide();

    patternPanel = new PatternPanel(0, off + tabsH - panelH, winW, panelH);
    patternPanel->setPattern(pattern);
    tab2->add(patternPanel);
    tab2->resizable(harmonyEd);

    // Stretch the editors down to the control panel so their vertical scrollbars
    // meet it (this resize also forces a relayout that fills them). The panel
    // may want more than panelH once it folds, so ask it rather than assume.
    tab2->onLayout               = [this] { layoutPatternTab(); };
    patternPanel->onHeightChanged = [this](int) { layoutPatternTab(); };
    layoutPatternTab();

    tabs->add(*tab2);

    // ---- Transport bar ----
    Fl_Group::current(nullptr);
    bottomPane = new Transport(0, off + tabsH, winW, bottomH, transport);
    window->add(bottomPane);
    if (disableTransportButtons)
        bottomPane->disableButtons();

    // ---- Wire up song editor ----
    og2->setTransport(transport, song);
    og2->onRulerOffsetChanged = [timeSigRuler, tempoRuler, loopRuler](int off, int clipLeft) {
        timeSigRuler->setOffsetX(off);
        timeSigRuler->setClipLeft(clipLeft);
        tempoRuler->setOffsetX(off);
        tempoRuler->setClipLeft(clipLeft);
        loopRuler->setOffsetX(off);
        loopRuler->setClipLeft(clipLeft);
    };
    og2->onNumColsChanged = [this, timeSigRuler, tempoRuler, loopRuler](int n) {
        timeSigRuler->setNumCols(n);
        tempoRuler->setNumCols(n);
        loopRuler->setNumCols(n);
        pushSongLoopState();   // the End marker may have been clamped in
    };
    // Horizontal zoom: the grid scales its bar width and the three rulers above
    // it follow, so markers and the loop region stay over the bars they mark.
    og2->onColWidthChanged = [timeSigRuler, tempoRuler, loopRuler](int cw) {
        timeSigRuler->setColWidth(cw);
        tempoRuler->setColWidth(cw);
        loopRuler->setColWidth(cw);
    };
    songPanel->onZoomChanged = [og2](float factor) { og2->setZoom(factor); };
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
    // Song-loop: the playhead asks (each tick, in song mode) whether the transport
    // loop toggle is on and, if so, for the loop region. Start is the left edge of
    // its column; End is right-aligned, so the loop ends at that column's right edge.
    og2->setPlayheadSongLoop([this, loopRuler](float& start, float& end) -> bool {
        // Song mode only: in Loop mode the patterns free-run against a linear clock
        // and the song playhead is frozen, so folding a position into the region
        // would misreport it. The RT sequencer gates the wrap the same way.
        if (!modeController.isSongMode()) return false;
        if (!bottomPane->loopEnabled()) return false;
        start = (float)loopRuler->startColumn();
        end   = (float)loopRuler->endColumn() + 1.0f;
        return true;
    });
    // Push the same region to the RT sequencer(s), which own the sample-accurate
    // loop wrap. The playhead closure above is only for the UI (visual + soft
    // ports); the toggle and marker drags feed this.
    bottomPane->onLoopToggled = [this](bool) { pushSongLoopState(); };
    loopRuler->onChanged      = [this]()     { pushSongLoopState(); };
    // In Song mode the rewind button jumps to the loop-ruler Start marker rather
    // than to bar 0 (in Loop mode the song playhead is frozen, so fall back).
    bottomPane->rewindTarget = [this, loopRuler](float& bar) -> bool {
        if (!modeController.isSongMode()) return false;
        bar = (float)loopRuler->startColumn();
        return true;
    };
    // After a rewind, scroll the song grid so the (now possibly off-screen)
    // playhead is visible — followPlayhead only chases it while playing, and we
    // want it in view when we switch back from the Loop/Pattern editor.
    bottomPane->onRewind = [this, og2, song, transport]() {
        if (modeController.isSongMode()) {
            og2->requestScrollToPlayhead();
            // A rewind is a reposition, so the tempo register re-reads the song's
            // markers where it landed — bar 0, or the loop-ruler Start marker above.
            // The transport has already moved by the time this runs. In Loop mode
            // the hold owns the tempo, and readTempoFromMap no-ops under it anyway.
            song->readTempoFromMap(transport->position());
        } else {
            // Loop mode: rewind put the transport back at bar 0, but each active
            // loop still carries the anchor it was switched on with, so its
            // playhead would land mid-pattern. Re-anchor them all to bar 0 so
            // every pattern restarts from its first beat.
            loopMgr.reanchorAll(0.0f);
        }
    };
    auto openPatternTab = [this, song, tab2](int trackIndex, int laneId) {
        song->selectLane(trackIndex, laneId);
        tabs->value(tab2);
        tabs->redraw();
        // Fl_Tabs::value() does not fire the widget callback, so the tab-change
        // hook never sees this. Without it, opening a pattern from the song editor
        // leaves the MIDI target on the tab we just left (i.e. nowhere) and the
        // keyboard does nothing. selectLane() above notifies too early to help:
        // it runs while the Song tab is still the current one.
        updateMidiTarget();
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
    if (instrRoute)
        og2->setPlayheadSoftRouting(instrRoute);

    // Clicking a pattern-editor row label auditions that note on the selected
    // track's instrument port.
    auditioner.setPortRegistry(portRegistry);
    auditioner.setInstrRoute(instrRoute);

    // ---- Wire up active pattern set ----
    loopEd->setLoopManager(&loopMgr);
    og2->setPlayheadLoopManager(&loopMgr);
    harmonyEd->setPlayheadLoopManager(&loopMgr);
    drumEd->setPlayheadLoopManager(&loopMgr);
    pianorollEd->setPlayheadLoopManager(&loopMgr);

    // ---- Wire up loop editor ----
    loopEd->setTimeline(song);
    loopEd->setPattern(pattern);
    loopEd->setSceneBank(&sceneBank);
    loopEd->setTransport(transport);
    loopEd->setContextPopup(loopCtxPop);
    loopEd->onSceneChosen = [this](int scene) { setScene(scene); };
    loopEd->onSceneEdited = [this](int) {
        // The scene's own set has already changed. It only has to reach the engine
        // when that scene is the one sounding.
        if (isLoopMode() && sceneBank.playingScene() == sceneBank.shownScene())
            applyShownScene();
        checkLoopStateChanged();
    };
    loopCtxPop->onOpenPattern     = openPatternTab;
    loopCtxPop->onShowInstruments = [this]() {
        if (outputsOverlay) outputsOverlay->show();
    };

    // The mode toggle keeps the currently-sounding loops alive across a switch: it
    // no longer clears loopMgr. The controller freezes the song playhead on
    // Song→Loop and does the bar-aligned seek-back on Loop→Song.
    modeController.init(transport, tabs, og2, [this](bool loop) {
        songEd->setPlayheadLoopMode(loop);
        harmonyEd->setPlayheadLoopMode(loop);
        drumEd->setPlayheadLoopMode(loop);
        pianorollEd->setPlayheadLoopMode(loop);
    });
    // Both halves of the saved loop state report through one hook: the mode when it
    // settles (which for Loop -> Song is the end of the hand-off, not the click),
    // and the switched-on set whenever the LoopManager changes.
    // Loop Mode holds the global tempo at the frozen bar, so the loops free-run at one
    // tempo and one time signature however long the jam lasts instead of drifting into
    // markers further down the song. The markers are untouched; they bound the tempo
    // again once the mode settles back to Song.
    modeController.setTempoFreeze = [this, song](bool loop, float atBar) {
        if (loop) song->holdTempo(atBar);
        else      song->releaseTempoHold();
        // The freeze notifies nobody by design; the Loop panel's BPM box is the one
        // thing that has to follow it, since it now speaks for the frozen bar.
        loopEd->refreshPanel();
    };
    modeController.onModeSettled = [this]() {
        // Entering Loop mode is when a user scene starts to matter. This runs after
        // setEditorsLoopMode() has gated sync() off — push any earlier and the song
        // would overwrite the scene on its next tick.
        //
        // Scene S is deliberately exempt. It is the song-linked scene, and the mode
        // toggle has always kept the currently-sounding loops alive across a switch
        // rather than reloading them; applying it here would clear the manual
        // overrides and re-anchor everything, which is the behaviour that predates
        // scenes and is not ours to change.
        if (isLoopMode() && SceneBank::isUserScene(sceneBank.shownScene()))
            applyShownScene();
        else
            sceneBank.setPlaying(sceneBank.shownScene());
        if (loopEd) loopEd->refreshSceneVisual();
        checkLoopStateChanged();
    };
    loopStateWatch.app = this;
    loopMgr.addObserver(&loopStateWatch);
    tabs->onModeChanged = [this](bool isLoop) {
        // A mode switch supersedes an armed scene change. The engine has one pending
        // slot and requestMode() is about to claim it, so the scene arm has to be
        // dropped here too or the UI would keep waiting for a switch the engine has
        // forgotten. The scene stays *shown*; onModeSettled applies it if the new
        // mode calls for it.
        loopMgr.cancelScene();
        modeController.requestMode(isLoop);
        // The transport loop toggle only applies in Song mode; grey it (but keep it
        // clickable) while in Loop mode.
        bottomPane->setLoopVisualDisabled(isLoop);
    };

    // ---- Wire up pattern editors ----
    harmonyEd->setPatternPlayhead(transport, pattern, 0);
    drumEd->setPatternPlayhead(transport, pattern, 0);
    pianorollEd->setPatternPlayhead(transport, pattern, 0);
    harmonyEd->setNoteLabelsContextPopup(nlCtxPop);
    drumEd->setNoteLabelsContextPopup(nlCtxPop);
    pianorollEd->setNoteLabelsContextPopup(nlCtxPop);
    harmonyEd->setParamLabelsContextPopup(nlCtxPop);
    drumEd->setParamLabelsContextPopup(nlCtxPop);
    pianorollEd->setParamLabelsContextPopup(nlCtxPop);
    // MIDI learn: the param-lane menus bind controls, and every param label shows
    // what is bound and what it last sent.
    nlCtxPop->midiLearn = &midiLearn;
    plcPop->midiLearn   = &midiLearn;
    for (BasePatternEditor* ed : {(BasePatternEditor*)harmonyEd, (BasePatternEditor*)drumEd,
                                  (BasePatternEditor*)pianorollEd})
        ed->setMidiLearn(&midiLearn);
    songEd->setMidiLearn(&midiLearn);
    midiLearn.onDisplayChanged = [this]() {
        for (BasePatternEditor* ed : {(BasePatternEditor*)harmonyEd, (BasePatternEditor*)drumEd,
                                      (BasePatternEditor*)pianorollEd})
            ed->redrawParamLabels();
        songEd->redrawTrackLabels();
    };
    midiLearn.onEdited = [this]() { if (onMidiLearnChanged) onMidiLearnChanged(); };

    harmonyEd->setParamDotPopup(pdPop);
    drumEd->setParamDotPopup(pdPop);
    pianorollEd->setParamDotPopup(pdPop);
    // The selection and paste menus are shared: only one editor is on screen at
    // a time, so one instance of each can serve them all.
    for (ISelectionHost* h : selectionHosts())
        if (h) { h->setSelectionPopup(selPop); h->setPastePopup(pastePop); }
    harmonyEd->setAuditioner(&auditioner);
    drumEd->setAuditioner(&auditioner);
    pianorollEd->setAuditioner(&auditioner);

    // ---- MIDI input ----
    // Everything arriving on any input lands here, on the UI thread, and anything
    // not from where the current instrument is played from is dropped straight
    // away — notes, controllers and MIDI learn alike. Notes go to whichever pattern editor is
    // showing, and are dropped when none is (the Song or Loop tab). Controllers are
    // never dropped: bound to a param lane they drive it, and unbound they are
    // forwarded to the instrument untouched, so the synth can learn them itself.
    midiIn.setSink([this](int slot, const uint8_t* data, int len) {
        // Recompute first. Every path that changes the visible editor is supposed
        // to call this, but a missed one would silently swallow MIDI rather than
        // fail visibly, so the cheap pointer compare is worth doing here too.
        updateMidiTarget();
        const bool accepted = midiInAccepted(slot, data[0]);
        if (luvieDebug())
            fprintf(stderr, "[luvie] midi in %d: %02X %02X%s target=%s%s\n", slot,
                    data[0], len > 1 ? data[1] : 0,
                    len > 2 ? " .." : "", midiTarget ? "yes" : "NONE",
                    accepted ? "" : " (not this instrument's input)");
        if (!accepted) return;
        if (len < 2) return;
        const int status = data[0] & 0xF0;

        // Controllers: MIDI learn decides what they are. Checked before the target,
        // because learning and the labels' live values work from any tab.
        const bool learnable = status == 0xB0 || status == 0xD0 || status == 0xE0;
        if (learnable) {
            std::string type;
            int         value = 0;
            if (midiLearn.handle(data, len, type, value)) {
                if (midiTarget) {
                    midiTarget->midiParam(type, value);
                } else {
                    // No pattern editor showing: still heard, on the selected track's
                    // instrument, so the control behaves the same from the Song tab.
                    const int instr = midiInInstrument();
                    if (instr >= 0) auditioner.param(instr, ccForType(type), value);
                }
                return;
            }
        }
        // Anything Luvie has no meaning for is passed to the instrument rather than
        // dropped: an unbound controller, and program change and poly aftertouch,
        // which have never had a lane here. Nothing records these and nothing
        // replays them, so unlike a note there is no live echo to suppress. The CC
        // number survives too — param() above remaps it through ccForType() — so
        // the synth sees what the controller sent and its MIDI learn can bind it.
        if (learnable || status == 0xA0 || status == 0xC0) {
            const int instr = midiInInstrument();
            if (instr < 0) {
                if (luvieDebug())
                    fprintf(stderr, "[luvie] passthru: no instrument (no editor "
                                    "showing and no track selected)\n");
                return;
            }
            auditioner.passThrough(instr, data, len);
            return;
        }
        if (!midiTarget) return;
        const int pitch  = data[1] & 0x7F;
        if (status == 0x90) {
            const int vel = (len >= 3) ? (data[2] & 0x7F) : 0;
            // Velocity 0 is a note-off by the running-status convention; the
            // editor handles that, so it is passed through as sent.
            midiTarget->midiNoteOn(pitch, vel);
        } else if (status == 0x80) {
            midiTarget->midiNoteOff(pitch);
        }
    });

    // A recorded note quantised just ahead of the playhead has already been heard
    // live; both sequencing paths drop that one firing. The RT engine plays Jack
    // ports under the Jack clock, the song playhead everything else.
    auto skipNoteOnce = [transport, og2](int instrumentId, int pitch, double bar, double tol) {
        transport->skipNoteOnce(instrumentId, pitch, bar, tol);
        og2->playheadSkipNoteOnce(instrumentId, pitch, (float)bar, (float)tol);
    };
    for (BasePatternEditor* ed : {(BasePatternEditor*)harmonyEd, (BasePatternEditor*)drumEd,
                                  (BasePatternEditor*)pianorollEd})
        ed->onSkipNoteOnce = skipNoteOnce;

    // The Record toggle arms whichever editor is currently the target.
    patternPanel->onRecordChanged = [this](bool on) {
        if (midiTarget) midiTarget->setRecordArmed(on);
        if (onRecordArmChanged) onRecordArmChanged();
    };

    // Grow goes to the same place, and shares Record's lifetime: it stays on between
    // takes but is cleared whenever the visible editor changes, so flexible bars are
    // always something the user has just asked for.
    patternPanel->onGrowChanged = [this](bool on) {
        if (midiTarget) midiTarget->setGrowArmed(on);
    };

    // Tab clicks fire nothing by default, so the target would go stale when the
    // user leaves the Pattern Editor tab. Fl_Tabs calls this on every change.
    tabs->callback([](Fl_Widget*, void* d) {
        static_cast<LuvieApp*>(d)->updateMidiTarget();
    }, this);
    updateMidiTarget();

    // Stopping ends the take: notes still held are committed with the length they
    // reached and the undo group closes, so the next run is its own undo entry.
    // Pause and rewind both come through here, and they disarm Record too: the next
    // take is something the user asks for again, not something play resumes.
    if (bottomPane) {
        bottomPane->onPlayStateChanged = [this](bool playing) {
            if (playing) return;
            stopMidiRecording();
            if (patternPanel) patternPanel->endTake();
        };
    }

    // ---- Note label / params sync ----
    auto syncHarmonyLabels = [this]() {
        harmonyEd->setNoteParams(patternPanel->rootPitch(),
                                 patternPanel->chordHash(),
                                 patternPanel->isSharp());
        if (onExtraParamsChanged) onExtraParamsChanged();
    };
    patternPanel->onParamsChanged = syncHarmonyLabels;
    patternPanel->onSnapChanged = [this](float s) {
        if (harmonyEd)   harmonyEd->setSnap(s);
        if (drumEd)      drumEd->setSnap(s);
        if (pianorollEd) pianorollEd->setSnap(s);
    };
    patternPanel->onDivisionsChanged = [this](int d) {
        if (harmonyEd)   harmonyEd->setDivisions(d);
        if (drumEd)      drumEd->setDivisions(d);
        if (pianorollEd) pianorollEd->setDivisions(d);
    };
    patternPanel->onZoomChanged = [this](float factor) {
        if (harmonyEd)   harmonyEd->setZoom(factor);
        if (drumEd)      drumEd->setZoom(factor);
        if (pianorollEd) pianorollEd->setZoom(factor);
    };
    patternPanel->onFocus = [this]() {
        if (drumEd && drumEd->visible())
            drumEd->focusPattern();
        else if (pianorollEd && pianorollEd->visible())
            pianorollEd->focusPattern();
        else if (harmonyEd)
            harmonyEd->focusPattern();
    };
    patternPanel->onRapidChanged = [this](bool r) {
        if (harmonyEd) harmonyEd->setRapidMode(r);
    };
    syncHarmonyLabels();

    // ---- Popups — added last (FLTK dispatches in reverse order) ----
    window->add(p1);     window->registerPopup(p1);
    window->add(p2);     window->registerPopup(p2);
    window->add(sp);     window->registerPopup(sp);
    window->add(selPop); window->registerPopup(selPop);
    window->add(pastePop); window->registerPopup(pastePop);
    window->add(tPop);   window->registerPopup(tPop);
    window->add(tsPop);  window->registerPopup(tsPop);
    window->add(ctxPop); window->registerPopup(ctxPop);
    window->add(ctxPop->paramSubmenu); window->registerPopup(ctxPop->paramSubmenu);
    window->add(loopCtxPop); window->registerPopup(loopCtxPop);
    window->add(loopRulerPop); window->registerPopup(loopRulerPop);
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
        outputsOverlay = new OutputsOverlay(om, oy, winW - 2*om, window->h() - oy - om,
                                            pluginMode);
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
        const int dlgH = 294;
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
    song_->addTempoObserver(&tempoNotifier_);

    // Wire pattern panel to instrument observable
    if (patternPanel)
        patternPanel->setInstruments(instruments_);

    // ---- Initial state ----
    // Seed with default instruments on a fresh (empty) session.
    if (song_->get().instruments.empty()) {
        const auto ports = outputsOverlay->getOutputs();
        // The melodic instrument takes the first port, the drumkit the second
        // (falling back to the first if only one port exists).
        const std::string port     = ports.empty() ? "" : ports[0];
        const std::string drumPort = ports.size() > 1 ? ports[1] : port;
        int id1 = instruments_->add("Instrument A", false);
        int id2 = instruments_->add("Drums A", true);
        outputsOverlay->setInstruments({
            {id1, "Instrument A", port,      1, {}, false, false, -1, -1, -1, -1},
            {id2, "Drums A",      drumPort, 10, {}, true,  false, -1, -1, -1, -1}
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
                    : pattern_->createHarmonyPattern(numPatternBeats, ci.id);
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
        song_->removeTempoObserver(&tempoNotifier_);
    }
    loopMgr.removeObserver(&loopStateWatch);
}

// ---- Persisted loop state ------------------------------------------------

bool LuvieApp::isLoopMode() const {
    return modeController.isLoopMode();
}

std::vector<int> LuvieApp::activeLoopPatterns() const {
    // Only Loop mode's set is ours to save; in Song mode the active patterns are
    // derived from the timeline by sync() and would be stale by the next bar.
    if (!isLoopMode()) return {};
    std::vector<int> ids;
    ids.reserve(loopMgr.patterns().size());
    for (const auto& [patId, anchor] : loopMgr.patterns()) ids.push_back(patId);
    // The manager keys off an unordered_map, so sort: an arbitrary order would
    // churn the saved JSON (and the plugin's state atom) with no real change.
    std::sort(ids.begin(), ids.end());
    return ids;
}

void LuvieApp::pushSongLoopState() {
    if (!onSongLoopChanged || !bottomPane || !loopRuler) return;
    // End marker is right-aligned to its column, so the loop end (exclusive) is the
    // column after it — matching the playhead closure in build().
    onSongLoopChanged(bottomPane->loopEnabled(),
                      (float)loopRuler->startColumn(),
                      (float)loopRuler->endColumn() + 1.0f);
}

void LuvieApp::songLoopState(bool& enabled, int& startCol, int& endCol) const {
    enabled  = bottomPane ? bottomPane->loopEnabled() : false;
    startCol = loopRuler ? loopRuler->startColumn() : 0;
    endCol   = loopRuler ? loopRuler->endColumn()   : -1;
}

void LuvieApp::applySongLoop(bool enabled, int startCol, int endCol) {
    if (loopRuler && endCol >= 0) {
        loopRuler->setStartColumn(startCol);
        loopRuler->setEndColumn(endCol);
    }
    if (bottomPane)
        bottomPane->setLoopEnabled(enabled);
    pushSongLoopState();
}

// LoopManager changed. Scene S is a mirror of it, so it follows — but only while
// Scene S is the scene on screen (SceneBank::mirrorSceneS enforces that), which is
// what freezes the mirror while another scene is showing.
void LuvieApp::onLoopsChanged() {
    sceneBank.mirrorSceneS(loopMgr.patterns());
    // An armed scene change has landed: the scene on screen is now the one sounding,
    // so the button stops being amber. Watching the manager for this keeps the UI
    // right whichever side landed it — the engine's seam or Playhead's crossing.
    if (!loopMgr.scenePending() && sceneBank.switchPending()) {
        sceneBank.setPlaying(sceneBank.shownScene());
        if (loopEd) loopEd->refreshSceneVisual();
    }
    checkLoopStateChanged();
}

void LuvieApp::checkLoopStateChanged() {
    if (applyingLoopState) return;
    bool             mode    = isLoopMode();
    std::vector<int> actives = activeLoopPatterns();
    auto             scenes  = sceneBank.save();
    int              shown   = sceneBank.shownScene();
    // Deliberately not Scene S's mirror: sync() churns it several times a bar in Song
    // mode, and comparing it here would mark the project dirty — and in plugin mode
    // re-send the whole JSON blob — on every bar of song playback.
    if (mode == savedLoopMode && actives == savedActiveLoopPatterns
        && scenes == savedScenes && shown == savedShownScene) return;
    savedLoopMode           = mode;
    savedActiveLoopPatterns = std::move(actives);
    savedScenes             = std::move(scenes);
    savedShownScene         = shown;
    if (onLoopStateChanged) onLoopStateChanged();
}

void LuvieApp::applyScenes(
        const std::array<std::vector<int>, SceneBank::kUserScenes>& sets, int shown) {
    applyingLoopState = true;
    sceneBank.load(sets, shown);
    // A scene saved against a pattern that has since been deleted would otherwise
    // keep switching on a block that is no longer there.
    if (song_) {
        std::set<int> live;
        for (const auto& p : song_->get().patterns) live.insert(p.id);
        sceneBank.prunePatterns(live);
    }
    // Loop mode with a user scene showing means that scene is what sounds. Scene S is
    // exempt: applyLoopState() has just restored the saved active set, and applying
    // the mirror over it would only re-anchor what is already right.
    if (isLoopMode() && SceneBank::isUserScene(sceneBank.shownScene()))
        applyShownScene();
    else
        sceneBank.setPlaying(sceneBank.shownScene());
    if (loopEd) { loopEd->refreshSceneVisual(); loopEd->redraw(); }
    applyingLoopState = false;

    // A load is not an edit — adopt the loaded values as the baseline.
    savedScenes     = sceneBank.save();
    savedShownScene = sceneBank.shownScene();
}

void LuvieApp::loopTimeSig(int& top, int& bottom, int& beatIdx) const {
    if (!song_) { top = -1; bottom = 4; beatIdx = 0; return; }
    timeSettings::BeatUnit beat;
    song_->loopTimeSigValue(top, bottom, beat);
    beatIdx = (int)beat;
}

void LuvieApp::applyLoopTimeSig(int top, int bottom, int beatIdx) {
    if (!song_) return;
    // mirrorLoopTimeSig, not setLoopTimeSig: a load must not re-anchor the transport
    // or fan out a tempo change — it is establishing the starting state, not editing.
    song_->mirrorLoopTimeSig(top, bottom, beatIdx);
    if (loopEd) loopEd->refreshPanel();
}

void LuvieApp::setScene(int scene) {
    // The editor has already made it the shown scene — the grid shows where we are
    // going. What is left is whether it sounds, which only Loop mode answers yes to.
    if (!isLoopMode()) {
        // Nothing is waiting on a bar line, so there is no pending state to show.
        sceneBank.setPlaying(scene);
        if (loopEd) loopEd->refreshSceneVisual();
        checkLoopStateChanged();
        return;
    }
    // Mid mode-switch the engine's one pending slot already holds the hand-off, and
    // arming a scene over it would drop that. The scene is shown now and applied when
    // the mode settles, which onModeSettled does.
    if (modeController.isTransitioning()) {
        if (loopEd) loopEd->refreshSceneVisual();
        checkLoopStateChanged();
        return;
    }
    applyShownScene();
    checkLoopStateChanged();
}

void LuvieApp::applyShownScene() {
    const int   scene = sceneBank.shownScene();
    const float pos   = transport_ ? std::max(0.0f, transport_->position()) : 0.0f;

    if (!transport_ || !transport_->isPlaying()) {
        // Stopped: there is no bar line to wait for, so it lands now.
        loopMgr.applySet(sceneBank.set(scene), pos);
        sceneBank.setPlaying(scene);
        if (loopEd) loopEd->refreshSceneVisual();
        return;
    }

    // Playing: the switch waits for the bar line so the change is musical. The engine
    // picks the frame — arming here and landing there is what keeps it beat-exact
    // however long the message takes to arrive (tens of ms in plugin mode). Until it
    // lands, the old scene keeps sounding while the grid already shows the new one,
    // and the scene button reads amber because shown != playing.
    const float atBar = std::ceil(pos - 1.0e-4f);
    loopMgr.armScene(sceneBank.set(scene), atBar);
    if (transport_) transport_->armScene(atBar);
    if (loopEd) loopEd->refreshSceneVisual();
}

void LuvieApp::applyLoopState(bool loopMode, const std::vector<int>& activePatterns) {
    applyingLoopState = true;
    // Mode first: it gates sync() off, so a Song-mode sync() can't overwrite the
    // set we are about to restore.
    modeController.setMode(loopMode);
    loopMgr.restore(loopMode ? activePatterns : std::vector<int>{}, 0.0f);
    if (bottomPane) bottomPane->setLoopVisualDisabled(loopMode);
    applyingLoopState = false;

    // A load is not an edit — adopt the loaded values as the baseline.
    savedLoopMode           = isLoopMode();
    savedActiveLoopPatterns = activeLoopPatterns();
}
