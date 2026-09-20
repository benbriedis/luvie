// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "timeline.hpp"
#include "midiBackend.hpp"
#include <array>
#include <map>
#include <string>
#include <vector>

struct JackOutput {
    std::string portName; // name of our registered MIDI output port
    MidiBackend backend = MidiBackend::Jack;  // where the port sends (Jack/Native/Debug)
};

// The project's single MIDI input. A struct rather than loose AppState fields so
// that allowing a second one later is a vector change, not a rename of everything
// that touches it. Jack/Native/Plugin only — Debug is an output-only sink.
struct MidiInput {
    MidiBackend backend = MidiBackend::Jack;
    int         channel = 0;   // 0 = Any; 1-16 = listen on that channel alone
};

// A hardware control, as MIDI learn identifies it. Always read on the input's
// configured channel, so the channel is not part of it. num is the CC number and
// is unused for the other kinds.
enum class MidiSrcKind { None, CC, PitchBend, Pressure };
struct MidiSrc {
    MidiSrcKind kind = MidiSrcKind::None;
    int         num  = 0;
    bool operator==(const MidiSrc& o) const {
        return kind == o.kind && (kind != MidiSrcKind::CC || num == o.num);
    }
    bool operator!=(const MidiSrc& o) const { return !(*this == o); }
};
// Param-lane type ("Modulation", "Pitch", ...) -> the control that drives it. One
// binding per type for the whole project: every pattern's lane of that type and the
// Song Editor's share it.
using MidiLearnBindings = std::map<std::string, MidiSrc>;
// What a new project starts with: the two controls nearly every keyboard has.
inline MidiLearnBindings defaultMidiLearnBindings() {
    return { {"Pitch",      {MidiSrcKind::PitchBend, 0}},
             {"Modulation", {MidiSrcKind::CC,        1}} };
}

struct JackInstrument {
    int         id                = 0;   // timeline Instrument ID (0 if unset)
    std::string name;
    std::string portName;
    int         midiChannel       = 1;
    std::map<int, std::string> drumMap;
    bool        isDrum            = false;
    bool        fallbackNoteNames = false;
    int         programNumber     = -1;  // -1 = not set; 0-127 = MIDI program
    int         bankMsb           = -1;  // -1 = not set; 0-127 = CC#0 value
    int         bankLsb           = -1;  // -1 = not set; 0-127 = CC#32 value
    int         gm1Instrument     = -1;  // last GM1 instrument selected from the dropdown
};

// App-level state that gets persisted to / loaded from disk.
struct AppState {
    Timeline timeline;
    int  transport = -1;  // clock source: 0=Host, 1=Internal, 2=Jack; -1=unset
    MidiBackend defaultPortBackend = MidiBackend::Jack;  // type assigned to newly added ports
    std::vector<JackOutput> jackOutputs;
    std::vector<JackInstrument> jackInstruments;
    MidiInput midiInput;
    // Starts at the defaults so a project saved before MIDI learn existed, which
    // has no "midiLearn" key, loads with them rather than with nothing bound.
    MidiLearnBindings midiLearn = defaultMidiLearnBindings();

    // Song/Loop mode, and in Loop mode which patterns the Loop Editor has switched
    // on. The LoopManager is otherwise runtime-only state, but these two survive so
    // a project reopens in the mode it was left in with the same loops running.
    // Anchors are deliberately not saved: on load every loop starts at bar 0.
    // activeLoopPatterns is meaningless (and left empty) in Song mode, where the
    // active set is derived from the timeline by LoopManager::sync().
    bool             loopMode = false;
    std::vector<int> activeLoopPatterns;   // pattern IDs, ascending

    // The Loop Editor's scenes. Scene S is not here: it is the song-linked scene, and
    // the set it shows is already saved as activeLoopPatterns above. Scenes 1-4 are
    // the user's own — the song never writes them — so each is its own pattern-id
    // list, ascending. Like the fields above these are otherwise runtime-only state,
    // and anchors are not saved here either, for the same reason.
    // A project saved before scenes existed loads with four empty scenes and Scene S
    // shown, which is exactly what those sessions were.
    std::array<std::vector<int>, 4> scenes;
    int currentScene = 0;   // 0 = Scene S, 1-4; the scene the Loop Editor shows

    // Loop Mode's own time signature, set in the Loop Editor. It decides how long a
    // Loop-Mode bar is — and so where a scene switch lands — independently of the
    // song's markers. top < 0 means "follow the song", which is what a project saved
    // before it existed loads as, so its Loop mode behaves exactly as before.
    int loopSigTop    = -1;
    int loopSigBottom = 4;
    int loopSigBeat   = 0;   // timeSettings::BeatUnit index

    // Song-mode loop: the song editor's Start/End markers + the loop toggle. Like
    // the fields above these are otherwise runtime-only, but persisting them lets a
    // project reopen with the same loop region armed. Columns are 0-based; End is
    // the last looped column (inclusive), matching LoopRuler. songLoopEndCol < 0
    // means "not saved" — leave the ruler at its default.
    bool             songLoopEnabled  = false;
    int              songLoopStartCol = 0;
    int              songLoopEndCol   = -1;
};

// Serialize/deserialize AppState to/from a JSON string.
std::string appStateToJsonString(const AppState& state);
bool appStateFromJsonString(const std::string& jsonStr, AppState& state);

// Save/load the full AppState to/from a JSON file.
// Return true on success.
bool saveAppState(const AppState& state, const std::string& filePath);
bool loadAppState(const std::string& filePath, AppState& state);
