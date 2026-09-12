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

// ── MIDI input ────────────────────────────────────────────────────────────────
// The MIDI input dropdown offers only these three: Debug is an output-only sink
// (it prints what would have been sent), so it is not an input a user can pick.
//
// Note the contrast with the port dropdown, where the item index IS the enum
// value. It cannot be here — Plugin is 3 but sits at item 2 once Debug is gone —
// so every read and write of the input Fl_Choice must go through the two helpers
// below rather than casting the index. backendSupported() above still applies
// unchanged: standalone drives Jack and Native, hosted drives Plugin.
inline constexpr MidiBackend kInputBackends[] = {
    MidiBackend::Jack, MidiBackend::Native, MidiBackend::Plugin };
inline constexpr int kNumInputBackends = 3;

// Dropdown item index for a backend, or -1 if it is not a valid input.
inline int inputBackendToIndex(MidiBackend b) {
    for (int i = 0; i < kNumInputBackends; i++)
        if (kInputBackends[i] == b) return i;
    return -1;
}

// The backend at a dropdown item index; Jack for anything out of range, which is
// also what a project carrying an input backend we cannot honour falls back to.
inline MidiBackend inputBackendFromIndex(int i) {
    if (i < 0 || i >= kNumInputBackends) return MidiBackend::Jack;
    return kInputBackends[i];
}

inline const char* inputBackendName(MidiBackend b) {
    switch (b) {
        case MidiBackend::Native: return "Native";
        case MidiBackend::Plugin: return "Plugin";
        default:                  return "Jack";
    }
}
