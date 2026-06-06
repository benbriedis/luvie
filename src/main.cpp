#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
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
#include "transportOverlay.hpp"
#include "drumPatternEditor.hpp"
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
                   "  --test           Use internal transport; skip JACK (implies --verbose)\n"
                   "  -h, --help       Show this help message\n\n"
                   "Arguments:\n"
                   "  project-file     Path to a .json project file to open on startup\n\n"
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
    bool            jackUp = false;   // set once JACK has been opened + wired up

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

    app.build(&window, &songTimeline, &patternObs, &instrObs, transport);

    // --- JACK port management ---------------------------------------------
    OutputsOverlay* connOverlay = app.outputsOverlay;

    // Helper: send program change (+ bank select) for all instruments that have one set.
    auto sendAllProgramChanges = [&]() {
        if (!jackUp || !connOverlay) return;
        for (const auto& ci : connOverlay->getInstruments()) {
            if (ci.programNumber < 0 && ci.bankMsb < 0 && ci.bankLsb < 0) continue;
            jackTransport.sendProgramChange(ci.portName, ci.midiChannel - 1,
                                            ci.bankMsb, ci.bankLsb, ci.programNumber);
        }
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
        connOverlay->onPortAdded = [&](const std::string& name) {
            if (jackUp) jackTransport.addMidiPort(name);
        };
        connOverlay->onPortRemoved = [&](const std::string& name) {
            if (jackUp) jackTransport.removeMidiPort(name);
        };
        connOverlay->onPortRenamed = [&](const std::string& oldName, const std::string& newName) {
            if (jackUp) jackTransport.renameMidiPort(oldName, newName);
        };
        connOverlay->onProgramChanged = [&](const std::string&) {
            if (app.onInstrumentsChanged) app.onInstrumentsChanged();
            sendAllProgramChanges();
        };
    }

    // Helper: unregister all current ports, then register ports from the overlay.
    auto applyLoadedOutputs = [&](const AppState& state) {
        if (!connOverlay) return;
        if (jackUp)
            for (const auto& name : connOverlay->getOutputs())
                jackTransport.removeMidiPort(name);
        std::vector<std::string> names;
        for (const auto& c : state.jackOutputs) names.push_back(c.portName);
        connOverlay->setOutputs(names);
        if (jackUp)
            for (const auto& name : connOverlay->getOutputs())
                jackTransport.addMidiPort(name);
        std::vector<OutputsOverlay::InstrumentInfo> instrs;
        for (const auto& c : state.jackInstruments)
            instrs.push_back({c.id, c.name, c.portName, c.midiChannel, c.drumMap,
                              c.isDrum, c.fallbackNoteNames, c.programNumber, c.bankMsb, c.bankLsb,
                              c.gm1Instrument});
        connOverlay->setInstruments(instrs);
        app.pushInstruments();
        if (app.onInstrumentsChanged) app.onInstrumentsChanged();
        sendAllProgramChanges();
    };

    // Helper: fill jackOutputs and jackInstruments in an AppState from the overlay.
    auto collectOutputs = [&](AppState& state) {
        if (!connOverlay) return;
        state.jackOutputs.clear();
        for (const auto& name : connOverlay->getOutputs())
            state.jackOutputs.push_back({name});
        state.jackInstruments.clear();
        for (const auto& ci : connOverlay->getInstruments())
            state.jackInstruments.push_back({ci.id, ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                                             ci.isDrum, ci.fallbackNoteNames,
                                             ci.programNumber, ci.bankMsb, ci.bankLsb,
                                             ci.gm1Instrument});
    };

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
        if (connOverlay)
            for (const auto& name : connOverlay->getOutputs())
                jackTransport.addMidiPort(name);
        if (app.onInstrumentsChanged) app.onInstrumentsChanged();
        sendAllProgramChanges();
    };

    // React to JACK availability changes from the observer.
    jackObserver.addListener([&](JackObserver::State s) {
        switch (s) {
        case JackObserver::State::Up:
            activateJack();
            router.setActive(&jackTransport);
            if (app.transportOverlay) app.transportOverlay->setJackWaiting(false);
            if (app.bottomPane) {
                app.bottomPane->setTransportWaiting(false);
                app.bottomPane->enableButtons();
                app.bottomPane->syncPlayState();
            }
            break;
        case JackObserver::State::Polling:
            // Waiting for the JACK server: disable the transport controls until
            // it comes up.
            router.setActive(&simpleTransport);
            if (app.transportOverlay) app.transportOverlay->setJackWaiting(true);
            if (app.bottomPane) {
                app.bottomPane->setTransportWaiting(true);
                app.bottomPane->disableButtons();
            }
            break;
        case JackObserver::State::Down:
            if (app.transportOverlay) app.transportOverlay->setJackWaiting(false);
            if (app.bottomPane) {
                app.bottomPane->setTransportWaiting(false);
                app.bottomPane->enableButtons();
            }
            break;
        }
    });

    if (app.transportOverlay) {
        app.transportOverlay->onTransportChanged = [&](int index) {
            // Changing the clock source: stop playback if it's running.
            if (router.isPlaying()) {
                router.pause();
                if (app.bottomPane) app.bottomPane->syncPlayState();
            }
            if (app.bottomPane) {
                static const char* names[] = {"Host", "Internal", "Jack"};
                if (index >= 0 && index < 3)
                    app.bottomPane->setTransportLabel(names[index]);
            }
            // 0 = Host (informational), 1 = Internal, 2 = Jack
            if (index == 2) {
                jackObserver.start();
            } else {
                jackObserver.stop();
                router.setActive(&simpleTransport);
                if (app.transportOverlay) app.transportOverlay->setJackWaiting(false);
                if (app.bottomPane)       app.bottomPane->setTransportWaiting(false);
            }
        };
        // --test forces the internal clock; otherwise default to JACK.
        app.transportOverlay->setSelection(testMode ? 1 : 2);
    }

    // --- NSM session management -------------------------------------------
    static NsmClient nsm;
    std::string nsmSessionPath;

    nsm.onOpen = [&](const std::string& path, const std::string& /*displayName*/) -> bool {
        nsmSessionPath = path;
        AppState state;
        if (loadAppState(path + ".json", state)) {
            songTimeline.loadTimeline(state.timeline);
            if (app.patternPanel)
                app.patternPanel->setParams(state.rootPitch, state.chordType, state.sharp);
            applyLoadedOutputs(state);
            if (state.transport >= 0 && !app.pluginMode && app.transportOverlay)
                app.transportOverlay->setSelection(state.transport);
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
        return saveAppState(state, nsmSessionPath + ".json");
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
        if (!projectPath.empty()) {
            AppState state;
            if (loadAppState(projectPath, state)) {
                songTimeline.loadTimeline(state.timeline);
                if (app.patternPanel)
                    app.patternPanel->setParams(state.rootPitch, state.chordType, state.sharp);
                applyLoadedOutputs(state);
                if (state.transport >= 0 && !app.pluginMode && app.transportOverlay)
                    app.transportOverlay->setSelection(state.transport);
            }
        }

        // Helper: shows a Save As dialog, returns chosen path (with .json),
        // or empty string if cancelled.
        auto pickSavePath = [&]() -> std::string {
            Fl_Native_File_Chooser fc;
            fc.title("Save Project As");
            fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
            fc.filter("JSON Files\t*.json\nAll Files\t*");
            fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
            if (!LuvieApp::lastFileDir.empty()) fc.directory(LuvieApp::lastFileDir.c_str());
            if (fc.show() != 0) return {};
            std::string p = fc.filename();
            if (p.size() < 5 || p.substr(p.size() - 5) != ".json") p += ".json";
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

    // When the window is closed save the session (NSM or standalone with a path).
    window.callback([](Fl_Widget* w, void*) {
        if (nsm.isActive() && nsm.onSave)
            nsm.onSave();
        w->hide();
    }, nullptr);

    // Arm FLTK's cross-thread wakeup so Fl::awake() callbacks posted from JACK's
    // threads (transport events, server-shutdown notification) are delivered to
    // the main thread.
    Fl::lock();

    window.show(argc, argv);
    return Fl::run();
}
