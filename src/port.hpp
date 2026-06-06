#pragma once
#include "midiBackend.hpp"
#include <string>
#include <utility>

// A single MIDI output port. Subclasses implement the actual sending:
//   JackPort  — JACK MIDI output (driven by the RT transport engine).
//   AlsaPort  — ALSA sequencer output.
//   DebugPort — prints to the console.
//
// All methods are called on the UI thread. Ports whose softSequenced() is true
// are driven note-by-note by the Playhead; Jack ports return false because their
// playback is generated sample-accurately on the JACK real-time thread instead.
class Port {
public:
    explicit Port(std::string name) : name_(std::move(name)) {}
    virtual ~Port() = default;

    const std::string& name() const { return name_; }
    virtual void rename(const std::string& n) { name_ = n; }

    virtual MidiBackend backend()       const = 0;
    virtual bool        softSequenced() const = 0;

    virtual void noteOn (int ch, int pitch, int vel) = 0;
    virtual void noteOff(int ch, int pitch)          = 0;
    virtual void cc     (int ch, int num, int val)   = 0;
    virtual void pitchBend(int ch, int value14)      = 0;
    virtual void programChange(int ch, int bankMsb, int bankLsb, int program) = 0;
    virtual void panic() {}   // all-notes-off; used by soft ports on stop

protected:
    std::string name_;
};
