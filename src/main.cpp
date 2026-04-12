#include <FL/Fl.H>
#include <string>
#include "appWindow.hpp"
#include "simpleTransport.hpp"
#include "jackTransport.hpp"
#include "observableTimeline.hpp"
#include "patternPanel.hpp"
#include "transport.hpp"
#include "noteLabels.hpp"
#include "luvieApp.hpp"
#include "nsm.hpp"
#include "timelineIO.hpp"

int main(int argc, char **argv) {
    bool verbose = false;
    int  fltk_argc = 1;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--verbose" || arg == "-v")
            verbose = true;
        else
            argv[fltk_argc++] = argv[i];
    }
    argc = fltk_argc;

    AppWindow window(LuvieApp::winW, LuvieApp::defaultWinH());
    window.color(bgColor);
    window.end();

    ObservableTimeline songTimeline(120.0f, 4, 4);
    for (int i = 1; i <= 8; i++) {
        int patId = songTimeline.createPattern(LuvieApp::numPatternBeats);
        songTimeline.addTrack("Pattern " + std::to_string(i), patId);
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

    if (useJack) {
        jackTransport.onTransportEvent = [&]() {
            Fl::awake([](void* data) {
                static_cast<Transport*>(data)->syncPlayState();
            }, app.bottomPane);
        };
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
        }
        // onOpen must succeed even if there is no existing file yet.
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
        return saveAppState(state, nsmSessionPath + ".json");
    };

    // Derive the executable basename for the announce message.
    std::string exeName = argv[0];
    auto slash = exeName.rfind('/');
    if (slash != std::string::npos) exeName = exeName.substr(slash + 1);

    nsm.init("Luvie", exeName.c_str());

    // When the window is closed directly (not via the session manager's save
    // + quit sequence), save the session so data isn't lost.
    window.callback([](Fl_Widget* w, void*) {
        if (nsm.isActive() && nsm.onSave)
            nsm.onSave();
        w->hide();
    }, nullptr);

    window.show(argc, argv);
    return Fl::run();
}
