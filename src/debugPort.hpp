#pragma once
#include "port.hpp"
#include <functional>

// A MIDI output port that prints messages to the console. Driven note-by-note by
// the Playhead (soft-sequenced), the same crossing path the --test/--verbose mode uses.
class DebugPort : public Port {
public:
    explicit DebugPort(std::string name) : Port(std::move(name)) {}

    MidiBackend backend()       const override { return MidiBackend::Debug; }
    bool        softSequenced() const override { return true; }

    // Optional: render a MIDI pitch as a note name (e.g. "E4"); set by the app.
    std::function<std::string(int)> pitchName;

    void noteOn (int ch, int pitch, int vel) override;
    void noteOff(int ch, int pitch)          override;
    void cc     (int ch, int num, int val)   override;
    void pitchBend(int ch, int value14)      override;
    void programChange(int ch, int bankMsb, int bankLsb, int program) override;
};
