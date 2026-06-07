#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <filesystem>
#include <map>
#include <string>
#include "appWindow.hpp"
#include "simpleTransport.hpp"
#include "jackTransport.hpp"
#include "transportRouter.hpp"
#include "jackObserver.hpp"
#include "observableSong.hpp"
#include "observablePattern.hpp"
#include "observableInstrument.hpp"
#include "patternPanel.hpp"
#include "transport.hpp"
#include "noteLabels.hpp"
#include "luvieApp.hpp"
#include "outputsOverlay.hpp"
#include "portRegistry.hpp"
#include "port.hpp"
#include "playhead.hpp"
#include "chords.hpp"
#include "transportOverlay.hpp"
#include "startupOverlay.hpp"
#include "drumPatternEditor.hpp"
#include "songEditor.hpp"
#include "itimelineobserver.hpp"
#include "nsm.hpp"
#include "timelineIO.hpp"

int main(int argc, char **argv) {
    bool verbose = false;
    bool testMode = false;
    std::string projectPath;   // optional CLI project file (standalone only)
    int  fltk_argc = 1;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printf("Usage: luvie [options] [project-file]\n\n"
                   "Options:\n"
                   "  --verbose        Print notes and parameter changes during playback\n"
                   "  --test           Use internal transport and debug MIDI output;\n"
                   "                   skip JACK (implies --verbose; no project file allowed)\n"
                   "  -h, --help       Show this help message\n\n"
                   "Arguments:\n"
                   "  project-file     Path to a .luv project file to open on startup\n\n"
                   "Environment:\n"
                   "  NSM_URL          Connect to a Non Session Manager at this OSC address\n");
            return 0;
        } else if (arg == "--verbose")
            verbose = true;
        else if (arg == "--test")
            testMode = verbose = true;
        else if (!arg.empty() && arg[0] != '-' && projectPath.empty())
            projectPath = arg;   // first non-flag arg treated as project file
        else
            argv[fltk_argc++] = argv[i];
    }
    argc = fltk_argc;

    // --test runs a self-contained internal session; pairing it with a named
    // project file on the command line is contradictory, so reject it.
    if (testMode && !projectPath.empty()) {
        fprintf(stderr,
                "Error: --test cannot be used together with a project file "
                "(\"%s\").\n", projectPath.c_str());
        return 1;
    }

    AppWindow window(LuvieApp::winW, LuvieApp::defaultWinH());
    window.color(bgColor);
    window.end();

    ObservableSong songTimeline(120.0f, 4, 4);
    ObservablePattern patternObs(&songTimeline);
    ObservableInstrument instrObs(&songTimeline);

    // The UI holds a single stable transport pointer (the router); we swap the
    // backing clock source (Internal vs JACK) behind it at runtime.
    // Quiet JACK's connection-failure logging during the availability poll,
    // unless the user asked for verbose output.
    if (!verbose) JackTransport::silenceLogging();

    JackTransport   jackTransport;
    SimpleTransport simpleTransport;
    TransportRouter router;
    JackObserver    jackObserver(&jackTransport);
    PortRegistry    portReg(&jackTransport);   // owns the live Port objects (Jack/Native/Debug)
    bool            jackUp = false;   // set once JACK has been opened + wired up
    bool            newProject = true;   // cleared once an existing project loads

    simpleTransport.setTimeline(&songTimeline);
    router.setActive(&simpleTransport);
    ITransport* transport = &router;

    LuvieApp app;
    app.verbose = verbose;

    if (verbose) {
        app.getPitchName = [&](int pitch) {
            return noteName(pitch, app.patternPanel->rootPitch(),
                                   app.patternPanel->chordType(),
                                   app.patternPanel->isSharp());
        };
    }

    app.onExtraParamsChanged = [&]() {
        if (jackUp)
            jackTransport.setNoteParams(app.patternPanel->rootPitch(),
                                        app.patternPanel->chordType());
    };

    // Soft (Native/Debug) output routing for the song playhead. Lambdas read live
    // state at playback time, so they're safe to set before build() populates widgets.
    app.portRegistry = &portReg;
    app.rowToMidi = [&app](int row) {
        return rowToMidi(row, app.patternPanel->rootPitch(), app.patternPanel->chordType());
    };
    app.instrRoute = [&app](int instrumentId) -> MidiInstrRoute {
        if (!app.outputsOverlay) return {};
        for (const auto& ci : app.outputsOverlay->getInstruments())
            if (ci.id == instrumentId) return { ci.portName, ci.midiChannel - 1 };
        return {};
    };

    app.build(&window, &songTimeline, &patternObs, &instrObs, transport);

    // --- JACK port management ---------------------------------------------
    OutputsOverlay* connOverlay = app.outputsOverlay;

    // Debug ports print note names; map a MIDI pitch → e.g. "C4".
    portReg.debugPitchName = [](int midi) -> std::string {
        static const char* names[12] = {"C","C#","D","D#","E","F",
                                         "F#","G","G#","A","A#","B"};
        if (midi < 0) midi = 0;
        return std::string(names[midi % 12]) + std::to_string(midi / 12 - 1);
    };

    // Helper: send program change (+ bank select) for all instruments that have one
    // set, through whichever backend their port uses.
    auto sendAllProgramChanges = [&]() {
        if (!connOverlay) return;
        for (const auto& ci : connOverlay->getInstruments()) {
            if (ci.programNumber < 0 && ci.bankMsb < 0 && ci.bankLsb < 0) continue;
            if (Port* p = portReg.find(ci.portName))
                p->programChange(ci.midiChannel - 1, ci.bankMsb, ci.bankLsb, ci.programNumber);
        }
    };

    // JACK is "wanted" (observer running) if the clock is Jack OR any port outputs to
    // Jack. --test skips JACK entirely (per --help).
    auto updateJackWanted = [&]() {
        if (testMode) { jackObserver.stop(); return; }
        bool wantJack = portReg.anyJack();
        if (app.transportOverlay && app.transportOverlay->selection() == 2) wantJack = true;
        if (wantJack) jackObserver.start();
        else          jackObserver.stop();
    };

    // Refresh the transport-bar alert indicator. Two JACK-related alerts can be
    // active; both clear once JACK connects, the clock source moves off Jack, or
    // the last Jack MIDI output goes away.
    auto updateAlerts = [&]() {
        if (!app.bottomPane) return;
        bool jackDown    = jackObserver.state() != JackObserver::State::Up;
        bool clockIsJack = app.transportOverlay && app.transportOverlay->selection() == 2;
        std::vector<std::string> alerts;
        if (clockIsJack && jackDown)
            alerts.push_back("Jack transport selected but JACK is not running");
        if (portReg.anyJack() && jackDown)
            alerts.push_back("Jack MIDI output in use but JACK is not running");
        app.bottomPane->setAlerts(alerts);
    };

    // Red "JACK server not running" line shows while a Jack port exists but JACK is down.
    auto updateJackWarning = [&]() {
        if (connOverlay)
            connOverlay->setJackWarning(portReg.anyJack() &&
                                        jackObserver.state() != JackObserver::State::Up);
        updateAlerts();
    };

    // Reconcile the Port set with the overlay, then refresh JACK want/warning state
    // and tell the song playhead whether any soft (Native/Debug) ports need driving.
    auto syncPorts = [&]() {
        if (!connOverlay) return;
        if (app.songEd) app.songEd->playheadPanicSoftNotes();   // release notes before teardown
        portReg.reconcile(connOverlay->getOutputsFull());
        if (app.songEd) {
            app.songEd->setPlayheadHasSoftPorts(portReg.anySoft());
            app.songEd->setPlayheadHasJackPorts(portReg.anyJack());
        }
        updateJackWanted();
        updateJackWarning();
    };

    // Tell the song playhead whether the Jack RT engine is the active clock. When it
    // isn't, the playhead soft-sequences Jack ports too; release held notes first so
    // none hang across the switch.
    auto applyClockMode = [&]() {
        if (!app.songEd) return;
        app.songEd->playheadPanicSoftNotes();
        app.songEd->setPlayheadJackClockActive(router.active() == &jackTransport);
    };

    // JACK-specific instrument callback: push routings to jackTransport.
    app.onInstrumentsChanged = [&]() {
        if (!jackUp || !connOverlay) return;
        std::vector<JackTransport::InstrumentRouting> routings;
        for (const auto& ci : connOverlay->getInstruments())
            routings.push_back({ci.id, ci.portName, ci.midiChannel,
                                ci.programNumber, ci.bankMsb, ci.bankLsb});
        jackTransport.setInstruments(routings);
    };

    if (connOverlay) {
        connOverlay->onPortAdded         = [&](const std::string&) { syncPorts(); };
        connOverlay->onPortRemoved       = [&](const std::string&) { syncPorts(); };
        connOverlay->onPortBackendChanged = [&]() { syncPorts(); };
        connOverlay->onPortRenamed = [&](const std::string& oldName, const std::string& newName) {
            portReg.rename(oldName, newName);
            if (app.onInstrumentsChanged) app.onInstrumentsChanged();
            updateJackWarning();
        };
        connOverlay->onProgramChanged = [&](const std::string&) {
            if (app.onInstrumentsChanged) app.onInstrumentsChanged();
            sendAllProgramChanges();
        };
    }

    // Helper: load ports + instruments from a saved state into the overlay and
    // reconcile the live Port set to match.
    auto applyLoadedOutputs = [&](const AppState& state) {
        if (!connOverlay) return;
        connOverlay->setDefaultBackend(state.defaultPortBackend);
        connOverlay->setOutputs(state.jackOutputs);
        std::vector<OutputsOverlay::InstrumentInfo> instrs;
        for (const auto& c : state.jackInstruments)
            instrs.push_back({c.id, c.name, c.portName, c.midiChannel, c.drumMap,
                              c.isDrum, c.fallbackNoteNames, c.programNumber, c.bankMsb, c.bankLsb,
                              c.gm1Instrument});
        connOverlay->setInstruments(instrs);
        app.pushInstruments();
        syncPorts();
        if (app.onInstrumentsChanged) app.onInstrumentsChanged();
        sendAllProgramChanges();
    };

    // Helper: fill jackOutputs and jackInstruments in an AppState from the overlay.
    auto collectOutputs = [&](AppState& state) {
        if (!connOverlay) return;
        state.defaultPortBackend = connOverlay->getDefaultBackend();
        state.jackOutputs = connOverlay->getOutputsFull();
        state.jackInstruments.clear();
        for (const auto& ci : connOverlay->getInstruments())
            state.jackInstruments.push_back({ci.id, ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                                             ci.isDrum, ci.fallbackNoteNames,
                                             ci.programNumber, ci.bankMsb, ci.bankLsb,
                                             ci.gm1Instrument});
    };

    // Let the menu's Import/Export include the outputs section.
    app.onApplyOutputs   = [&](const AppState& state) { applyLoadedOutputs(state); };
    app.onCollectOutputs = [&](AppState& state)       { collectOutputs(state); };

    // --- Transport selection ----------------------------------------------
    // One-time JACK wiring, run when the server first becomes available.
    auto activateJack = [&]() {
        if (jackUp) return;
        jackUp = true;
        jackTransport.setTimeline(&songTimeline);
        jackTransport.setActivePatterns(&app.aps);
        jackTransport.setNoteParams(app.patternPanel->rootPitch(),
                                    app.patternPanel->chordType());
        jackTransport.onTransportEvent = [&]() {
            Fl::awake([](void* data) {
                static_cast<Transport*>(data)->syncPlayState();
            }, app.bottomPane);
        };
        // Server vanished: clear jackUp so a reconnect re-runs this wiring (ports,
        // instruments, program changes) on the fresh client, and hand off to the
        // observer to drop back to polling.
        jackTransport.onShutdown = [&]() {
            jackUp = false;
            jackObserver.serverLost();
        };
        portReg.reregisterJack();   // (re)register Jack ports on the fresh client
        if (app.onInstrumentsChanged) app.onInstrumentsChanged();
        sendAllProgramChanges();
    };

    // React to JACK availability changes from the observer. The clock/transport-UI
    // effects only apply when the *clock source* is Jack; a Jack output port alone
    // keeps the observer running (to register ports + warn) without hijacking the clock.
    jackObserver.addListener([&](JackObserver::State s) {
        bool clockIsJack = app.transportOverlay && app.transportOverlay->selection() == 2;
        switch (s) {
        case JackObserver::State::Up:
            activateJack();
            if (clockIsJack) {
                router.setActive(&jackTransport);
                applyClockMode();
                if (app.transportOverlay) app.transportOverlay->setJackWaiting(false);
                if (app.bottomPane) {
                    app.bottomPane->enableButtons();
                    app.bottomPane->syncPlayState();
                }
            }
            break;
        case JackObserver::State::Polling:
            // Waiting for the JACK server: disable the transport controls until
            // it comes up (only relevant when the clock is Jack).
            if (clockIsJack) {
                router.setActive(&simpleTransport);
                applyClockMode();
                if (app.transportOverlay) app.transportOverlay->setJackWaiting(true);
                if (app.bottomPane) {
                    app.bottomPane->disableButtons();
                }
            }
            break;
        case JackObserver::State::Down:
            if (clockIsJack) {
                if (app.transportOverlay) app.transportOverlay->setJackWaiting(false);
                if (app.bottomPane) {
                    app.bottomPane->enableButtons();
                }
            }
            break;
        }
        updateJackWarning();
    });

    if (app.transportOverlay) {
        app.transportOverlay->onTransportChanged = [&](int index) {
            // Changing the clock source: stop playback if it's running.
            if (router.isPlaying()) {
                router.pause();
                if (app.bottomPane) app.bottomPane->syncPlayState();
            }
            // 0 = Host (informational), 1 = Internal, 2 = Jack
            if (index == 2) {
                updateJackWanted();   // ensure JACK is up; Up-listener wires the clock
            } else {
                router.setActive(&simpleTransport);
                applyClockMode();
                updateJackWanted();   // keep JACK only if an output port still needs it
                if (app.transportOverlay) app.transportOverlay->setJackWaiting(false);
                if (app.bottomPane) {
                    app.bottomPane->enableButtons();   // were disabled while polling JACK
                }
            }
            updateJackWarning();
        };
        // --test forces the internal clock; otherwise default to JACK.
        app.transportOverlay->setSelection(testMode ? 1 : 2);
    }

    // --test also routes MIDI to the debug console by default: make Debug the
    // default backend for new ports and retype any ports already present.
    if (testMode && connOverlay) {
        connOverlay->setDefaultBackend(MidiBackend::Debug);
        auto outs = connOverlay->getOutputsFull();
        for (auto& o : outs) o.backend = MidiBackend::Debug;
        connOverlay->setOutputs(outs);
    }

    // Initial reconcile of the default port set (and JACK want/warning state).
    syncPorts();

    // --- New-project startup dialog ---------------------------------------
    // Applies its two choices live to the existing overlays. The MIDI-output
    // choice sets the default for new ports and also retypes the ports already
    // present so the pick takes effect on this fresh project.
    if (app.startupOverlay) {
        app.startupOverlay->onTransportChanged = [&](int index) {
            if (app.transportOverlay) app.transportOverlay->setSelection(index);
        };
        app.startupOverlay->onDefaultBackendChanged = [&](MidiBackend backend) {
            if (!connOverlay) return;
            connOverlay->setDefaultBackend(backend);
            auto outs = connOverlay->getOutputsFull();
            for (auto& o : outs) o.backend = backend;
            connOverlay->setOutputs(outs);
            syncPorts();
        };
    }

    // Pop the new-project dialog, seeded with the current defaults. Shown for a
    // fresh standalone project (below) and for a fresh NSM session (from onOpen).
    auto showStartupDialog = [&]() {
        if (app.pluginMode || !app.startupOverlay) return;
        app.startupOverlay->setSelections(
            app.transportOverlay ? app.transportOverlay->selection() : 2,
            connOverlay ? connOverlay->getDefaultBackend() : MidiBackend::Jack);
        app.startupOverlay->show();
    };

    // --- NSM session management -------------------------------------------
    static NsmClient nsm;
    std::string nsmSessionPath;

    nsm.onOpen = [&](const std::string& path, const std::string& /*displayName*/) -> bool {
        nsmSessionPath = path;
        AppState state;
        if (loadAppState(path + ".luv", state)) {
            newProject = false;
            songTimeline.loadTimeline(state.timeline);
            if (app.patternPanel)
                app.patternPanel->setParams(state.rootPitch, state.chordType, state.sharp);
            applyLoadedOutputs(state);
            if (state.transport >= 0 && !app.pluginMode && app.transportOverlay)
                app.transportOverlay->setSelection(state.transport);
        } else {
            // Fresh NSM session — no saved file yet; let the user pick defaults.
            showStartupDialog();
        }
        return true;
    };

    nsm.onSave = [&]() -> bool {
        if (nsmSessionPath.empty()) return false;
        AppState state;
        state.timeline  = songTimeline.get();
        if (app.patternPanel) {
            state.rootPitch = app.patternPanel->rootPitch();
            state.chordType = app.patternPanel->chordType();
            state.sharp     = app.patternPanel->isSharp();
        }
        if (app.transportOverlay) state.transport = app.transportOverlay->selection();
        collectOutputs(state);
        return saveAppState(state, nsmSessionPath + ".luv");
    };

    std::string exeName = argv[0];
    auto slash = exeName.rfind('/');
    if (slash != std::string::npos) exeName = exeName.substr(slash + 1);

    nsm.init("Luvie", exeName.c_str());

    // --- Wire Save / Save As based on mode --------------------------------
    fprintf(stderr, "[luvie] nsm.isActive() = %d\n", (int)nsm.isActive());
    if (nsm.isActive()) {
        // NSM manages the session file; Save As makes no sense here.
        app.disableSaveMenu(/*save=*/false, /*saveAs=*/true);
        app.onSave = [&]() { if (nsm.onSave) nsm.onSave(); };

    } else {
        // Standalone mode: load from CLI path if provided, then set up Save.

        // Helper: shows a Save As dialog, returns chosen path (with .luv),
        // or empty string if cancelled.
        auto pickSavePath = [&]() -> std::string {
            Fl_Native_File_Chooser fc;
            fc.title("Save Project As");
            fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
            fc.filter("Luvie Projects\t*.luv\nAll Files\t*");
            fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
            if (!LuvieApp::lastFileDir.empty()) fc.directory(LuvieApp::lastFileDir.c_str());
            if (fc.show() != 0) return {};
            std::string p = fc.filename();
            if (p.size() < 4 || p.substr(p.size() - 4) != ".luv") p += ".luv";
            LuvieApp::lastFileDir = std::filesystem::path(p).parent_path().string();
            return p;
        };

        // Helper: writes the current session to path.
        auto saveToPath = [&](const std::string& path) {
            AppState state;
            state.timeline = songTimeline.get();
            if (app.patternPanel) {
                state.rootPitch = app.patternPanel->rootPitch();
                state.chordType = app.patternPanel->chordType();
                state.sharp     = app.patternPanel->isSharp();
            }
            if (app.transportOverlay) state.transport = app.transportOverlay->selection();
            collectOutputs(state);
            saveAppState(state, path);
        };

        // Load the CLI project file if given. If it doesn't exist yet, create an
        // empty project at that path so the user can start a new project there.
        if (!projectPath.empty()) {
            AppState state;
            if (loadAppState(projectPath, state)) {
                newProject = false;
                songTimeline.loadTimeline(state.timeline);
                if (app.patternPanel)
                    app.patternPanel->setParams(state.rootPitch, state.chordType, state.sharp);
                applyLoadedOutputs(state);
                if (state.transport >= 0 && !app.pluginMode && app.transportOverlay)
                    app.transportOverlay->setSelection(state.transport);
            } else if (!std::filesystem::exists(projectPath)) {
                saveToPath(projectPath);   // new project: write out the empty default
            } else {
                fprintf(stderr, "[luvie] invalid project file: %s\n",
                        projectPath.c_str());
                fl_alert("Invalid project file");
                return 1;
            }
        }

        app.onSaveAs = [&, pickSavePath, saveToPath]() mutable {
            std::string p = pickSavePath();
            if (p.empty()) return;
            projectPath = p;
            saveToPath(projectPath);
        };

        app.onSave = [&, pickSavePath, saveToPath]() mutable {
            if (projectPath.empty()) {
                // No file yet — behave as Save As.
                std::string p = pickSavePath();
                if (p.empty()) return;
                projectPath = p;
            }
            saveToPath(projectPath);
        };
    }

    // On window close, prompt whether to save before quitting. The window
    // callback must be a plain function pointer, so the bits it needs are bundled
    // in a context passed via the data pointer (valid for all of Fl::run()).
    struct CloseCtx {
        NsmClient*            nsm;
        std::function<void()> save;   // performs a save in the current mode
    };
    CloseCtx closeCtx;
    closeCtx.nsm  = &nsm;
    closeCtx.save = [&]() {
        if (nsm.isActive()) { if (nsm.onSave) nsm.onSave(); }
        else if (app.onSave) app.onSave();
    };

    window.callback([](Fl_Widget* w, void* data) {
        auto* ctx = static_cast<CloseCtx*>(data);

        // Under NSM the session manager owns the session file, so just save and go.
        if (ctx->nsm->isActive()) {
            ctx->save();
            w->hide();
            return;
        }

        // Standalone: ask the user what to do with the session.
        // b0 is the default (also fired by Esc / WM close), so make it Cancel.
        int choice = fl_choice("Save this session before closing?",
                               "Cancel", "Don't Save", "Save");
        if (choice == 0) return;           // Cancel — keep the window open
        if (choice == 2) ctx->save();      // Save, then close
        w->hide();                         // Save or Don't Save both close
    }, &closeCtx);

    // Arm FLTK's cross-thread wakeup so Fl::awake() callbacks posted from JACK's
    // threads (transport events, server-shutdown notification) are delivered to
    // the main thread.
    Fl::lock();

    window.show(argc, argv);

    // For a fresh standalone project, prompt for the transport + default MIDI
    // output before the user starts. (Fresh NSM sessions get the same dialog from
    // the NSM open handler, once the session path is known.) --test picks its
    // own defaults (internal transport + debug output), so skip the dialog.
    if (newProject && !nsm.isActive() && !testMode)
        showStartupDialog();

    return Fl::run();
}
