// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>
#include <FL/Fl_Group.H>
#include "editor.hpp"
#include "itransport.hpp"
#include "itimelineobserver.hpp"
#include "loopManager.hpp"
#include "loopModeController.hpp"
#include "noteAuditioner.hpp"

struct AppState;
class ObservableSong;
class ObservablePattern;
class ObservableInstrument;

class AppWindow;
class ISelectionHost;
class ModernTabs;
class SettingsButton;
class SettingsMenuPopup;
class SongEditor;
class PortRegistry;
class HarmonyEditor;
class DrumPatternEditor;
class PianorollEditor;
class PatternPanel;
class SongPanel;
class LoopEditor;
class LoopRuler;
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
    static constexpr int numPatternBeats = 4;  // default new-pattern length: 1 bar in 4/4
    static constexpr int panelH          = 32;
    static constexpr int rowHeight       = 30;

    // Sized so both tabs get their full body: ten 45px song rows (or the pattern
    // editor's rows) above a one-row control bar.
    static int defaultWinH() {
        return tabBarH + 3*markerRulerH + Editor::rulerH + 10*45 + 20 + panelH + bottomH;
    }

    // Options — set before calling build()
    bool verbose                            = false;
    bool disableTransportButtons            = false;
    bool pluginMode                         = false;  // true when hosted as an LV2 plugin
    std::function<std::string(int)> getPitchName;
    // Soft (Native/Debug) MIDI output routing for the song playhead.
    PortRegistry*                      portRegistry = nullptr;
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

    // Fires when the *persisted* part of the loop state changes: the Song/Loop mode,
    // or which patterns are switched on while in Loop mode. Deliberately narrower
    // than a LoopManager observer — sync() churns the active set several times a bar
    // in Song mode and none of that is saved, so observing the manager directly
    // would mark the project dirty (or re-send the whole session) continuously.
    std::function<void()> onLoopStateChanged;

    // Fires when the song-loop toggle or the Start/End markers change:
    // (enabled, startBar, endBar) in song-bar units with endBar exclusive. Wire to
    // the RT sequencer(s) (JackTransport::setSongLoop / the plugin loop atom) so the
    // loop wrap is applied sample-accurately rather than by a UI-timer seek.
    std::function<void(bool enabled, float startBar, float endBar)> onSongLoopChanged;

    // Re-send the current song-loop region through onSongLoopChanged. Call once
    // after wiring the callback, and whenever it must be re-synced (e.g. JACK
    // reconnect). No-op until build() has run.
    void pushSongLoopState();

    // The current song-loop toggle + Start/End marker columns (0-based, End
    // inclusive), for persisting to AppState. No-op until build() has run.
    void songLoopState(bool& enabled, int& startCol, int& endCol) const;

    // Restore the song-loop toggle + markers from a loaded project, then push the
    // region to the RT sequencer(s). endCol < 0 leaves the markers untouched (a
    // project saved before the region was persisted).
    void applySongLoop(bool enabled, int startCol, int endCol);

    // The persisted loop state — see AppState::loopMode / activeLoopPatterns.
    bool             isLoopMode() const;
    std::vector<int> activeLoopPatterns() const;   // ascending; empty in Song mode

    // Restore both from a loaded project. Not treated as an edit: the loaded values
    // become the new baseline rather than firing onLoopStateChanged.
    void applyLoopState(bool loopMode, const std::vector<int>& activePatterns);

    // Drives the Song/Loop mode toggle: freezes the song playhead in loop mode and
    // performs the bar-aligned hand-off back to song mode. Wired in build().
    LoopModeController modeController;

    // Auditions single notes when a pattern-editor row label is clicked.
    NoteAuditioner auditioner;

    // Widgets — valid after build()
    SettingsButton*    settingsButton = nullptr;
    SettingsMenuPopup* settingsMenu   = nullptr;
    ModernTabs*        tabs         = nullptr;
    Fl_Group*          patternTab   = nullptr;
    HarmonyEditor*     harmonyEd    = nullptr;
    DrumPatternEditor* drumEd       = nullptr;
    PianorollEditor*   pianorollEd  = nullptr;
    PatternPanel*      patternPanel = nullptr;
    SongEditor*        songEd       = nullptr;
    SongPanel*         songPanel    = nullptr;
    LoopEditor*        loopEd       = nullptr;
    LoopRuler*         loopRuler    = nullptr;
    Transport*         bottomPane   = nullptr;
    OutputsOverlay*    outputsOverlay = nullptr;
    TransportOverlay*  transportOverlay = nullptr;
    StartupOverlay*    startupOverlay = nullptr;

    void build(AppWindow* window, ObservableSong* song, ObservablePattern* pattern,
               ObservableInstrument* instruments, ITransport* transport);
    void pushInstruments();

    // Fits the pattern editors above the control panel, whose height varies with
    // how many rows the panel has folded into.
    void layoutPatternTab();

private:
    // Every grid that can hold a multi-selection. Entries are null before build()
    // has created that editor.
    std::array<ISelectionHost*, 4> selectionHosts() const;

    // Watches the LoopManager and the mode controller, and reports through
    // onLoopStateChanged only when the saved values actually differ.
    struct LoopStateWatch : ILoopObserver {
        LuvieApp* app = nullptr;
        void onLoopsChanged() override { app->checkLoopStateChanged(); }
    };
    LoopStateWatch   loopStateWatch;
    bool             savedLoopMode = false;
    std::vector<int> savedActiveLoopPatterns;
    bool             applyingLoopState = false;   // suppresses reporting during a load
    void checkLoopStateChanged();

    bool layingOutPatternTab = false;

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
