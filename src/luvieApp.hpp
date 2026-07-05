#pragma once
#include <functional>
#include <string>
#include <FL/Fl_Group.H>
#include "editor.hpp"
#include "itransport.hpp"
#include "itimelineobserver.hpp"
#include "loopManager.hpp"
#include "noteAuditioner.hpp"

struct AppState;
class ObservableSong;
class ObservablePattern;
class ObservableInstrument;

class AppWindow;
class ModernTabs;
class SettingsButton;
class SettingsMenuPopup;
class SongEditor;
class PortRegistry;
class PatternEditor;
class DrumPatternEditor;
class PianorollEditor;
class PatternPanel;
class LoopEditor;
class Transport;
class NoteContextPopup;
class MarkerPopup;
class TrackContextPopup;
class OutputsOverlay;
class TransportOverlay;
class StartupOverlay;

// Builds and wires the shared Luvie UI layout (tabs, editors, transport bar, popups).
// Callers create AppWindow, ObservableSong, ObservablePattern, and ITransport,
// configure the optional callbacks, then call build().
class LuvieApp {
public:
    LuvieApp() = default;
    LuvieApp(const LuvieApp&) = delete;
    LuvieApp& operator=(const LuvieApp&) = delete;
    ~LuvieApp();

    // Layout constants
    static constexpr int tabBarH         = 35;
    static constexpr int bottomH         = 50;
    static constexpr int markerRulerH    = 18;
    static constexpr int winW            = 920;
    static constexpr int numPatternBeats = 8;
    static constexpr int panelH          = 32;
    static constexpr int rowHeight       = 30;

    static int defaultWinH() {
        return tabBarH + 2*markerRulerH + Editor::rulerH + 10*45 + 20 + bottomH;
    }

    // Options — set before calling build()
    bool verbose                            = false;
    bool disableTransportButtons            = false;
    bool pluginMode                         = false;  // true when hosted as an LV2 plugin
    std::function<std::string(int)> getPitchName;
    // Soft (Native/Debug) MIDI output routing for the song playhead.
    PortRegistry*                      portRegistry = nullptr;
    std::function<int(int,int,int)>    rowToMidi;    // (row, root, chord) → MIDI pitch
    std::function<MidiInstrRoute(int)> instrRoute;   // instrument id → port/channel
    std::function<void()>           onExtraSeek;
    std::function<void()>           onExtraParamsChanged;
    std::function<void()>           onExtraTimelineChange;
    std::function<void()>           onInstrumentsChanged;

    static std::string lastFileDir;  // remembered across Save As / Import / Export

    // Set before or after build() to wire up Save As. onSaveAs is called when the
    // Save As menu item is chosen. disableSaveMenu() greys it out; call after build().
    std::function<void()> onSaveAs;
    void disableSaveMenu(bool saveAs);

    // Outputs (ports/instruments) persistence — wired by main so Import/Export
    // include the outputs section. onCollectOutputs fills state from the overlay
    // for Export; onApplyOutputs pushes a loaded state into the overlay on Import.
    std::function<void(AppState&)>       onCollectOutputs;
    std::function<void(const AppState&)> onApplyOutputs;

    // Active pattern state — wire external consumers (e.g. JackTransport) to this after build().
    LoopManager loopMgr;

    // Auditions single notes when a pattern-editor row label is clicked.
    NoteAuditioner auditioner;

    // Widgets — valid after build()
    SettingsButton*    settingsButton = nullptr;
    SettingsMenuPopup* settingsMenu   = nullptr;
    ModernTabs*        tabs         = nullptr;
    Fl_Group*          patternTab   = nullptr;
    PatternEditor*     patternEd    = nullptr;
    DrumPatternEditor* drumEd       = nullptr;
    PianorollEditor*   pianorollEd  = nullptr;
    PatternPanel*      patternPanel = nullptr;
    SongEditor*        songEd       = nullptr;
    LoopEditor*        loopEd       = nullptr;
    Transport*         bottomPane   = nullptr;
    OutputsOverlay*    outputsOverlay = nullptr;
    TransportOverlay*  transportOverlay = nullptr;
    StartupOverlay*    startupOverlay = nullptr;

    void build(AppWindow* window, ObservableSong* song, ObservablePattern* pattern,
               ObservableInstrument* instruments, ITransport* transport);
    void pushInstruments();

private:
    ObservableSong*      song_        = nullptr;
    ObservablePattern*   pattern_     = nullptr;
    ObservableInstrument* instruments_ = nullptr;

    static void saveAsCb    (Fl_Widget*, void* data);
    static void importCb    (Fl_Widget*, void* data);
    static void exportCb    (Fl_Widget*, void* data);
    static void outputsCb   (Fl_Widget*, void* data);
    static void transportCb (Fl_Widget*, void* data);

    struct EditorSwitcher : ITimelineObserver {
        LuvieApp* app;
        explicit EditorSwitcher(LuvieApp* a) : app(a) {}
        void onTimelineChanged() override;
    } editorSwitcher_{this};

    struct ChangeNotifier : ITimelineObserver {
        LuvieApp* app;
        explicit ChangeNotifier(LuvieApp* a) : app(a) {}
        void onTimelineChanged() override;
    } changeNotifier_{this};
};
