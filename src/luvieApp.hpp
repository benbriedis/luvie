#pragma once
#include <functional>
#include <string>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Bar.H>
#include "editor.hpp"
#include "itransport.hpp"
#include "itimelineobserver.hpp"

class ObservableTimeline;

class AppWindow;
class ModernTabs;
class PatternEditor;
class DrumPatternEditor;
class PatternPanel;
class LoopEditor;
class Transport;
class Popup;
class MarkerPopup;
class TrackContextPopup;

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

    // Widgets — valid after build()
    Fl_Menu_Bar*       menuBar      = nullptr;
    ModernTabs*        tabs         = nullptr;
    Fl_Group*          patternTab   = nullptr;
    PatternEditor*     patternEd    = nullptr;
    DrumPatternEditor* drumEd       = nullptr;
    PatternPanel*      patternPanel = nullptr;
    LoopEditor*        loopEd       = nullptr;
    Transport*         bottomPane   = nullptr;

    void build(AppWindow* window, ObservableTimeline* timeline, ITransport* transport);

private:
    ObservableTimeline* timeline_ = nullptr;

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
