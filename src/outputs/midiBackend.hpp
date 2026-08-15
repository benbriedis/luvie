// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <string>

// Where a MIDI output port sends its events.
//   Jack   — a JACK MIDI output port, driven by the real-time transport engine.
//   Native — a native MIDI port via RtMidi (ALSA on Linux, CoreMIDI on macOS).
//   Debug  — print note/CC messages to the console.
//   Plugin — one of the LV2 plugin's own MIDI output ports (hosted mode only).
//
// The enum order is the dropdown order in OutputsOverlay (index == enum value),
// so new backends must be appended, never inserted.
enum class MidiBackend { Jack, Native, Debug, Plugin };

// Backends a given run mode can actually drive. The others stay visible but
// greyed in the port dropdown: a project moved between standalone and plugin
// keeps its settings, it just cannot use the ones this mode has no output for.
inline bool backendSupported(MidiBackend b, bool pluginMode) {
    return pluginMode ? (b == MidiBackend::Plugin) : (b != MidiBackend::Plugin);
}

// The backend newly added ports get when the stored default is unusable here.
inline MidiBackend defaultBackendFor(bool pluginMode) {
    return pluginMode ? MidiBackend::Plugin : MidiBackend::Jack;
}

inline const char* backendToString(MidiBackend b) {
    switch (b) {
        case MidiBackend::Native: return "native";
        case MidiBackend::Debug:  return "debug";
        case MidiBackend::Plugin: return "plugin";
        case MidiBackend::Jack:   break;
    }
    return "jack";
}

inline MidiBackend backendFromString(const std::string& s) {
    if (s == "native") return MidiBackend::Native;
    if (s == "debug")  return MidiBackend::Debug;
    if (s == "plugin") return MidiBackend::Plugin;
    return MidiBackend::Jack;
}
