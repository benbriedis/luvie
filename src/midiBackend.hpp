#pragma once
#include <string>

// Where a MIDI output port sends its events.
//   Jack   — a JACK MIDI output port, driven by the real-time transport engine.
//   Native — an ALSA sequencer output port.
//   Debug  — print note/CC messages to the console.
enum class MidiBackend { Jack, Native, Debug };

inline const char* backendToString(MidiBackend b) {
    switch (b) {
        case MidiBackend::Native: return "native";
        case MidiBackend::Debug:  return "debug";
        case MidiBackend::Jack:   break;
    }
    return "jack";
}

inline MidiBackend backendFromString(const std::string& s) {
    if (s == "native") return MidiBackend::Native;
    if (s == "debug")  return MidiBackend::Debug;
    return MidiBackend::Jack;
}
