// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <FL/Fl_Group.H>
#include "editor.hpp"
#include "itransport.hpp"
#include "itimelineobserver.hpp"
#include "sceneBank.hpp"
#include "loopManager.hpp"
#include "loopModeController.hpp"
#include "noteAuditioner.hpp"
#include "patternRecorder.hpp"
#include "midiInPort.hpp"
#include "midiLearn.hpp"
#include "parameterSubmenu.hpp"   // ParamMenuActions
#include "sceneTriggers.hpp"

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
class BasePatternEditor;

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
    // The global tempo register changed (IGlobalTempoObserver). Separate from
    // onExtraTimelineChange on purpose: the register is not project content, so
    // the plugin ships it in the small live loop atom rather than re-serializing
    // the whole song. See ObservableSong's "Global tempo" block.
    std::function<void()>           onGlobalTempoChanged;
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

    // The Loop Editor's scenes. A store of sets that feeds loopMgr; the engine never
    // learns what a scene is. See sceneBank.hpp.
    SceneBank   sceneBank;

    // Show a scene in the Loop Editor and, if it is Loop mode's turn to sound, make
    // loopMgr hold it. Silent in Song mode, where the song still drives playback.
    void setScene(int scene);
    // Push the shown scene into loopMgr. Called when a scene is chosen in Loop mode,
    // and when the mode settles into Loop with a scene already showing.
    void applyShownScene();
    // The set a scene would sound, as loopMgr wants it.
    const std::set<int>& sceneSet(int scene) const { return sceneBank.set(scene); }

    // The persisted scene state — see AppState::scenes / currentScene. Scene S is not
    // among them: it is a mirror of loopMgr, already saved as activeLoopPatterns.
    std::array<std::vector<int>, SceneBank::kUserScenes> sceneSets() const {
        return sceneBank.save();
    }
    int  shownScene() const { return sceneBank.shownScene(); }
    // Restore from a loaded project, dropping any pattern that no longer exists. Not
    // treated as an edit: the loaded values become the new baseline. Call after
    // applyLoopState(), so the mode has settled before a scene is shown.
    void applyScenes(const std::array<std::vector<int>, SceneBank::kUserScenes>& sets,
                     int shown);

    // Loop Mode's own time signature — see AppState::loopSigTop and
    // ObservableSong::setLoopTimeSig. top < 0 means "follow the song".
    void loopTimeSig(int& top, int& bottom, int& beatIdx) const;
    void applyLoopTimeSig(int top, int bottom, int beatIdx);

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

    // Auditions single notes when a pattern-editor row label is clicked, and when
    // a note arrives on the MIDI input.
    NoteAuditioner auditioner;

    // The pattern editor incoming MIDI is going to, or null when the Song or Loop
    // tab is showing. Owned by the tab group, not by us.
    BasePatternEditor* midiTarget = nullptr;

    // The project's MIDI inputs. Lives here rather than in main.cpp because the
    // plugin UI needs it too: hosted, nothing is opened and the LV2 port_event
    // feeds it directly, but the sink and the routing below are the same either way.
    MidiInputManager midiIn;

    // Which hardware control drives each param-lane type, shared by every pattern and
    // the Song Editor. Saved with the project (AppState::midiLearn) but kept out of
    // the timeline, so undo never changes a binding.
    MidiLearnMap midiLearn;
    // What the param-lane menus do with instrument parameters (edit, learn new).
    ParamMenuActions paramActions;
    // The MIDI triggers that switch the Loop Editor's scenes and toggle its patterns.
    // Saved with the project (AppState::sceneTriggers, AppState::patternTriggers),
    // and like midiLearn kept out of the timeline.
    SceneTriggerMap sceneTriggers;
    // From a loaded project, after its timeline: triggers for patterns it no longer
    // has are dropped.
    void applyTriggers(const AppState& state);
    // Show the transport buttons' triggers in their tooltips and outline the one
    // learning. Called whenever the trigger map's display changes.
    void refreshTransportTriggers();
    // The instrument whose notes set the Harmony Editor's base note, or -1. Chosen
    // from the base note's right-click menu and saved with the project
    // (AppState::harmonyRootTrigger); like the triggers above, kept out of the timeline.
    int  harmonyRootTrigger() const { return harmonyRootTrigger_; }
    // From a loaded project, or the menu. Only the menu is an edit: `edited` fires
    // onMidiLearnChanged.
    void setHarmonyRootTrigger(int instrId, bool edited = false);
    // The user bound or cleared a control or a scene trigger: the project needs
    // saving (standalone), or the state re-sending to the DSP (plugin). Loading
    // bindings does not fire it.
    std::function<void()> onMidiLearnChanged;
    // A MIDI input was renamed. The input manager follows it, and so do the scene
    // triggers learned on it.
    void midiInputRenamed(const std::string& oldName, const std::string& newName);

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

    // ── MIDI input routing ───────────────────────────────────────────────────
    // Incoming MIDI lights the rows of whichever pattern editor is on screen.
    // Separately, every pattern with Record armed records its own instrument's
    // input, whatever is on screen, and every instrument with Pass through on
    // sounds what is played on its input, unquantised — so several can be played
    // and recorded at once, from different inputs, channels or sides of a split.
    // The Song and Loop tabs are not targets, but armed patterns keep recording
    // from them.
    //
    // Call after anything that can change which editor is visible — a tab switch,
    // or a selection change that swaps one pattern editor for another.
    void updateMidiTarget();
    // The instrument incoming MIDI sounds on: the visible pattern editor's, or the
    // selected track's when the Song or Loop tab is showing, so a control behaves
    // the same from every tab. -1 when there is neither.
    int midiInInstrument() const;
    // Whether a message arriving on input `slot` is from where midiInInstrument()
    // is played from — its input, on its channel, and for a note on its side of the
    // keyboard split. Anything else is ignored — notes and unbound controllers
    // alike — except by MIDI learn and the scene triggers, which listen to every
    // input. True when there is no such instrument.
    bool midiInAccepted(int slot, const uint8_t* data, int len) const;
    // midiInAccepted() for any instrument: true when it has no input set, else
    // instrumentHears().
    bool instrumentAccepts(int instrId, int slot, const uint8_t* data, int len) const;
    // The same test for any instrument: whether a message arriving on `slot` is on
    // instrument `instrId`'s input, channel and side of its split. False when there
    // is no such instrument.
    bool instrumentHears(int instrId, int slot, const uint8_t* data, int len) const;
    // Every instrument with Pass through on that hears a message arriving on
    // `slot` (instrumentHears()): where it is sent straight on to, as played.
    std::vector<int> passThroughInstruments(int slot, const uint8_t* data, int len) const;
    // Is any pattern armed to record?
    bool anyRecordArmed() const { return recorders.anyRecordArmed(); }
    // Fired whenever a Record toggle arms or disarms, including every one the
    // transport stopping disarms.
    std::function<void()> onRecordArmChanged;

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
        void onLoopsChanged() override { app->onLoopsChanged(); }
    };
    LoopStateWatch   loopStateWatch;
    bool             savedLoopMode = false;
    std::vector<int> savedActiveLoopPatterns;
    std::array<std::vector<int>, SceneBank::kUserScenes> savedScenes;
    int              savedShownScene   = 0;
    bool             applyingLoopState = false;   // suppresses reporting during a load
    void checkLoopStateChanged();
    void onLoopsChanged();

    // Every pattern with Record or Grow armed. Session state, never saved.
    PatternRecorders recorders;
    // Puts the shown pattern's arm states on the panel's toggles.
    void syncArmButtons();
    // The instruments each held key was sent to, so its note-off reaches the same
    // ones even if what is on screen, or what is armed, has changed since. Keyed
    // by input slot, channel and pitch (heldKey).
    std::map<int, std::vector<int>> heldNotes_;
    static int heldKey(int slot, const uint8_t* data) {
        return (slot << 16) | ((data[0] & 0x0F) << 8) | (data[1] & 0x7F);
    }

    bool layingOutPatternTab = false;
    int  harmonyRootTrigger_ = -1;
    // Set while an edit moves a parameter's control itself (a rename), so the song's
    // onParamNamesChanged leaves the controls alone.
    bool paramNamesMuted_    = false;

    ObservableSong*      song_        = nullptr;
    ObservablePattern*   pattern_     = nullptr;
    ObservableInstrument* instruments_ = nullptr;
    ITransport*          transport_   = nullptr;

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

    struct TempoNotifier : IGlobalTempoObserver {
        LuvieApp* app;
        explicit TempoNotifier(LuvieApp* a) : app(a) {}
        void onGlobalTempoChanged() override {
            if (app->onGlobalTempoChanged) app->onGlobalTempoChanged();
        }
    } tempoNotifier_{this};
};
