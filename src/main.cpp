#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <filesystem>
#include <map>
#include <string>
#include "appWindow.hpp"
#include "simpleTransport.hpp"
#include "jackTransport.hpp"
#include "observableSong.hpp"
#include "observablePattern.hpp"
#include "patternPanel.hpp"
#include "transport.hpp"
#include "noteLabels.hpp"
#include "luvieApp.hpp"
#include "outputsOverlay.hpp"
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

    struct TimelineWatcher : ITimelineObserver {
        std::function<void()> onChange;
        void onTimelineChanged() override { if (onChange) onChange(); }
    };
    TimelineWatcher timelineWatcher;

    AppWindow window(LuvieApp::winW, LuvieApp::defaultWinH());
    window.color(bgColor);
    window.end();

    ObservableSong songTimeline(120.0f, 4, 4);
    songTimeline.defaultOutputInstrument = "Instrument 1";
    {
        int patId = songTimeline.createPattern(LuvieApp::numPatternBeats);
        songTimeline.addTrack("Pattern 1", patId);
    }
    ObservablePattern patternObs(&songTimeline);

    JackTransport  jackTransport;
    SimpleTransport simpleTransport;
    bool useJack = !testMode && jackTransport.open();
    ITransport* transport = useJack
        ? static_cast<ITransport*>(&jackTransport)
        : static_cast<ITransport*>(&simpleTransport);
    if (useJack)
        jackTransport.setTimeline(&songTimeline);
    else
        simpleTransport.setTimeline(&songTimeline);

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
        if (useJack)
            jackTransport.setNoteParams(app.patternPanel->rootPitch(),
                                        app.patternPanel->chordType());
    };

    app.build(&window, &songTimeline, &patternObs, transport);

    if (useJack)
        jackTransport.setActivePatterns(&app.aps);

    if (useJack) {
        jackTransport.onTransportEvent = [&]() {
            Fl::awake([](void* data) {
                static_cast<Transport*>(data)->syncPlayState();
            }, app.bottomPane);
        };
    }

    // --- JACK port management ---------------------------------------------
    OutputsOverlay* connOverlay = app.outputsOverlay;

    // Helper: push current instrument list to jackTransport and patternPanel.
    auto pushInstrumentRoutings = [&]() {
        if (!connOverlay) return;
        const auto& instrs = connOverlay->getInstruments();
        // Update timeline defaults to first instrument of each type.
        songTimeline.defaultOutputInstrument     = "";
        songTimeline.defaultDrumOutputInstrument = "";
        for (const auto& ci : instrs) {
            if (!ci.isDrum && songTimeline.defaultOutputInstrument.empty())
                songTimeline.defaultOutputInstrument = ci.name;
            if (ci.isDrum && songTimeline.defaultDrumOutputInstrument.empty())
                songTimeline.defaultDrumOutputInstrument = ci.name;
        }
        if (useJack) {
            std::vector<JackTransport::InstrumentRouting> routings;
            for (const auto& ci : instrs)
                routings.push_back({ci.name, ci.portName, ci.midiChannel,
                                    ci.programNumber, ci.bankMsb, ci.bankLsb});
            jackTransport.setInstruments(routings);
        }
        if (app.patternPanel) {
            std::vector<std::string> stdNames, drumNames;
            for (const auto& ci : instrs)
                (ci.isDrum ? drumNames : stdNames).push_back(ci.name);
            app.patternPanel->setInstruments(stdNames, drumNames);
        }
    };

    // Helper: send program change (+ bank select) for all instruments that have one set.
    auto sendAllProgramChanges = [&]() {
        if (!useJack || !connOverlay) return;
        for (const auto& ci : connOverlay->getInstruments()) {
            if (ci.programNumber < 0 && ci.bankMsb < 0 && ci.bankLsb < 0) continue;
            jackTransport.sendProgramChange(ci.portName, ci.midiChannel - 1,
                                            ci.bankMsb, ci.bankLsb, ci.programNumber);
        }
    };

    // Helper: push drum maps from instruments to drum editor.
    auto pushDrumMaps = [&]() {
        if (!connOverlay || !app.drumEd) return;
        std::map<std::string, std::map<int, std::string>> allMaps;
        std::map<std::string, bool> allFallbacks;
        for (const auto& ci : connOverlay->getInstruments()) {
            allMaps[ci.name]      = ci.drumMap;
            allFallbacks[ci.name] = ci.fallbackNoteNames;
        }
        app.drumEd->setAllDrumMaps(allMaps, allFallbacks);
    };

    if (connOverlay) {
        connOverlay->onPortAdded = [&](const std::string& name) {
            if (useJack) jackTransport.addMidiPort(name);
        };
        connOverlay->onPortRemoved = [&](const std::string& name) {
            if (useJack) jackTransport.removeMidiPort(name);
        };
        connOverlay->onPortRenamed = [&](const std::string& oldName, const std::string& newName) {
            if (useJack) jackTransport.renameMidiPort(oldName, newName);
        };
        connOverlay->onInstrumentRenamed = [&](const std::string& oldName, const std::string& newName) {
            songTimeline.renamePatternOutputInstrument(oldName, newName);
        };
        connOverlay->isInstrumentInUse = [&](const std::string& name) {
            for (const auto& p : songTimeline.get().patterns)
                if (p.outputInstrumentName == name) return true;
            return false;
        };
        connOverlay->onInstrumentsChanged = [&]() {
            pushInstrumentRoutings();
            pushDrumMaps();
            connOverlay->refreshInstrumentButtons();
        };
        connOverlay->onProgramChanged = [&](const std::string&) {
            pushInstrumentRoutings();
            sendAllProgramChanges();
        };

        timelineWatcher.onChange = [&]() { connOverlay->refreshInstrumentButtons(); };
        songTimeline.addObserver(&timelineWatcher);

        // Register the default port and push initial instrument routings.
        if (useJack) {
            for (const auto& name : connOverlay->getOutputs())
                jackTransport.addMidiPort(name);
        }
        pushInstrumentRoutings();
        pushDrumMaps();
    }

    if (app.drumEd) {
        app.drumEd->onDrumLabelChanged = [&](const std::string& instrName, int midiNote, const std::string& label) {
            if (connOverlay) connOverlay->updateInstrumentDrumMap(instrName, midiNote, label);
        };
    }

    // Helper: unregister all current ports, then register ports from the overlay.
    auto applyLoadedOutputs = [&](const AppState& state) {
        if (!connOverlay) return;
        // Unregister existing ports
        if (useJack)
            for (const auto& name : connOverlay->getOutputs())
                jackTransport.removeMidiPort(name);
        // Load new port list
        std::vector<std::string> names;
        for (const auto& c : state.jackOutputs) names.push_back(c.portName);
        connOverlay->setOutputs(names);
        // Register loaded ports
        if (useJack)
            for (const auto& name : connOverlay->getOutputs())
                jackTransport.addMidiPort(name);
        // Load instruments and push routings.
        std::vector<OutputsOverlay::InstrumentInfo> instrs;
        for (const auto& c : state.jackInstruments)
            instrs.push_back({0, c.name, c.portName, c.midiChannel, c.drumMap,
                              c.isDrum, c.fallbackNoteNames, c.programNumber, c.bankMsb, c.bankLsb,
                              c.gm1Instrument});
        connOverlay->setInstruments(instrs);
        pushInstrumentRoutings();
        pushDrumMaps();
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
            state.jackInstruments.push_back({ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                                             ci.isDrum, ci.fallbackNoteNames,
                                             ci.programNumber, ci.bankMsb, ci.bankLsb,
                                             ci.gm1Instrument});
    };

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

    window.show(argc, argv);
    return Fl::run();
}
