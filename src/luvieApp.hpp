#pragma once
#include <functional>
#include <string>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Bar.H>
#include "editor.hpp"
#include "itransport.hpp"
#include "itimelineobserver.hpp"
#include "activePatternSet.hpp"

class ObservableTimeline;

class AppWindow;
class ModernTabs;
class PatternEditor;
class DrumPatternEditor;
class PianorollEditor;
class PatternPanel;
class LoopEditor;
class Transport;
class Popup;
class MarkerPopup;
class TrackContextPopup;
class OutputsOverlay;

// Builds and wires the shared Luvie UI layout (tabs, editors, transport bar, popups).
// Callers create AppWindow, ObservableTimeline, and ITransport, configure the optional
// callbacks, then call build().
class LuvieApp {
public:
    LuvieApp() = default;
    LuvieApp(const LuvieApp&) = delete;
    LuvieApp& operator=(const LuvieApp&) = delete;
    ~LuvieApp();

    // Layout constants
    static constexpr int menuBarH        = 22;
    static constexpr int tabBarH         = 35;
    static constexpr int bottomH         = 50;
    static constexpr int markerRulerH    = 18;
    static constexpr int winW            = 920;
    static constexpr int numPatternBeats = 8;
    static constexpr int panelH          = 50;
    static constexpr int rowHeight       = 30;

    static int defaultWinH() {
        return menuBarH + tabBarH + 2*markerRulerH + Editor::rulerH + 10*45 + 20 + bottomH;
    }

    // Options — set before calling build()
    bool verbose                            = false;
    bool disableTransportButtons            = false;
    std::function<std::string(int)> getPitchName;
    std::function<void()>           onExtraSeek;
    std::function<void()>           onExtraParamsChanged;
    std::function<void()>           onExtraTimelineChange;

    static std::string lastFileDir;  // remembered across Save As / Import / Export

    // Set before or after build() to wire up Save / Save As.
    // onSave / onSaveAs are called when the matching menu item is chosen.
    // disableSaveMenu() greys out the items; call after build().
    std::function<void()> onSave;
    std::function<void()> onSaveAs;
    void disableSaveMenu(bool save, bool saveAs);

    // Active pattern state — wire external consumers (e.g. JackTransport) to this after build().
    ActivePatternSet aps;

    // Widgets — valid after build()
    Fl_Menu_Bar*       menuBar      = nullptr;
    ModernTabs*        tabs         = nullptr;
    Fl_Group*          patternTab   = nullptr;
    PatternEditor*     patternEd    = nullptr;
    DrumPatternEditor* drumEd       = nullptr;
    PianorollEditor*   pianorollEd  = nullptr;
    PatternPanel*      patternPanel = nullptr;
    LoopEditor*        loopEd       = nullptr;
    Transport*            bottomPane          = nullptr;
    OutputsOverlay*   outputsOverlay  = nullptr;

    void build(AppWindow* window, ObservableTimeline* timeline, ITransport* transport);

private:
    ObservableTimeline* timeline_ = nullptr;

    static void saveCb   (Fl_Widget*, void* data);
    static void saveAsCb (Fl_Widget*, void* data);
    static void importCb (Fl_Widget*, void* data);
    static void exportCb      (Fl_Widget*, void* data);
    static void outputsCb (Fl_Widget*, void* data);

    struct EditorSwitcher : ITimelineObserver {
        LuvieApp* app;
        explicit EditorSwitcher(LuvieApp* a) : app(a) {}
        void onTimelineChanged() override;
    } editorSwitcher_{this};

    struct ChangeNotifier : ITimelineObserver {
        LuvieApp* app;
        explicit ChangeNotifier(LuvieApp* a) : app(a) {}
        void onTimelineChanged() override {
            if (app->onExtraTimelineChange) app->onExtraTimelineChange();
        }
    } changeNotifier_{this};
};
