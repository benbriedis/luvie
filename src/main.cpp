#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <filesystem>
#include <map>
#include <string>
#include "appWindow.hpp"
#include "simpleTransport.hpp"
#include "jackTransport.hpp"
#include "observableTimeline.hpp"
#include "patternPanel.hpp"
#include "transport.hpp"
#include "noteLabels.hpp"
#include "luvieApp.hpp"
#include "connectionsOverlay.hpp"
#include "drumPatternEditor.hpp"
#include "itimelineobserver.hpp"
#include "nsm.hpp"
#include "timelineIO.hpp"

int main(int argc, char **argv) {
    bool verbose = false;
    std::string projectPath;   // optional CLI project file (standalone only)
    int  fltk_argc = 1;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--verbose" || arg == "-v")
            verbose = true;
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

    ObservableTimeline songTimeline(120.0f, 4, 4);
    songTimeline.defaultOutputChannel = "midi_out_1:1";
    {
        int patId = songTimeline.createPattern(LuvieApp::numPatternBeats);
        songTimeline.addTrack("Pattern 1", patId);
    }

    JackTransport  jackTransport;
    SimpleTransport simpleTransport;
    bool useJack = jackTransport.open();
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

    app.build(&window, &songTimeline, transport);

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
    ConnectionsOverlay* connOverlay = app.connectionsOverlay;

    // Helper: push current channel list to jackTransport and patternPanel.
    auto pushChannelRoutings = [&]() {
        if (!connOverlay) return;
        const auto& chans = connOverlay->getChannels();
        if (!chans.empty())
            songTimeline.defaultOutputChannel = chans[0].name;
        if (useJack) {
            std::vector<JackTransport::ChannelRouting> routings;
            for (const auto& ci : connOverlay->getChannels())
                routings.push_back({ci.name, ci.portName, ci.midiChannel});
            jackTransport.setChannels(routings);
        }
        if (app.patternPanel) {
            std::vector<std::string> names;
            for (const auto& ci : connOverlay->getChannels())
                names.push_back(ci.name);
            app.patternPanel->setChannels(names);
        }
    };

    // Helper: push drum maps from channels to drum editor.
    auto pushDrumMaps = [&]() {
        if (!connOverlay || !app.drumEd) return;
        std::map<std::string, std::map<int, std::string>> allMaps;
        for (const auto& ci : connOverlay->getChannels())
            allMaps[ci.name] = ci.drumMap;
        app.drumEd->setAllDrumMaps(allMaps);
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
        connOverlay->onChannelRenamed = [&](const std::string& oldName, const std::string& newName) {
            songTimeline.renamePatternOutputChannel(oldName, newName);
        };
        connOverlay->isChannelInUse = [&](const std::string& name) {
            for (const auto& p : songTimeline.get().patterns)
                if (p.outputChannelName == name) return true;
            return false;
        };
        connOverlay->onChannelsChanged = [&]() {
            pushChannelRoutings();
            pushDrumMaps();
            connOverlay->refreshChannelButtons();
        };

        timelineWatcher.onChange = [&]() { connOverlay->refreshChannelButtons(); };
        songTimeline.addObserver(&timelineWatcher);

        // Register the default port and push initial channel routings.
        if (useJack) {
            for (const auto& name : connOverlay->getConnections())
                jackTransport.addMidiPort(name);
        }
        pushChannelRoutings();
        pushDrumMaps();
    }

    // Helper: unregister all current ports, then register ports from the overlay.
    auto applyLoadedConnections = [&](const AppState& state) {
        if (!connOverlay) return;
        // Unregister existing ports
        if (useJack)
            for (const auto& name : connOverlay->getConnections())
                jackTransport.removeMidiPort(name);
        // Load new port list
        std::vector<std::string> names;
        for (const auto& c : state.jackConnections) names.push_back(c.portName);
        connOverlay->setConnections(names);
        // Register loaded ports
        if (useJack)
            for (const auto& name : connOverlay->getConnections())
                jackTransport.addMidiPort(name);
        // Load channels and push routings.
        std::vector<ConnectionsOverlay::ChannelInfo> chans;
        for (const auto& c : state.jackChannels)
            chans.push_back({0, c.name, c.portName, c.midiChannel, c.drumMap});
        connOverlay->setChannels(chans);
        pushChannelRoutings();
        pushDrumMaps();
    };

    // Helper: fill jackConnections and jackChannels in an AppState from the overlay.
    auto collectConnections = [&](AppState& state) {
        if (!connOverlay) return;
        state.jackConnections.clear();
        for (const auto& name : connOverlay->getConnections())
            state.jackConnections.push_back({name});
        state.jackChannels.clear();
        for (const auto& ci : connOverlay->getChannels())
            state.jackChannels.push_back({ci.name, ci.portName, ci.midiChannel, ci.drumMap});
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
            applyLoadedConnections(state);
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
        collectConnections(state);
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
                applyLoadedConnections(state);
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
            collectConnections(state);
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
