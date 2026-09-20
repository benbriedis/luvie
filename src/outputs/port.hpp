// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "midiBackend.hpp"
#include <cstdint>
#include <string>
#include <utility>

// A single MIDI output port. Subclasses implement the actual sending:
//   JackPort    — JACK MIDI output (driven by the RT transport engine).
//   RtMidiPort  — native MIDI output via RtMidi (ALSA / CoreMIDI).
//   DebugPort   — prints to the console.
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

    // One complete channel message, sent as given. For MIDI input pass-through,
    // where the point is that the synth sees the bytes the controller sent — so its
    // own MIDI learn can bind them — and for the messages the typed methods above
    // cannot express at all (channel pressure, poly aftertouch). The channel is
    // taken from msg[0]; the caller has already rewritten it to the instrument's.
    //
    // The default decodes what the typed methods do cover and drops the rest, so a
    // port only overrides this if it can emit arbitrary bytes.
    virtual void raw(const uint8_t* msg, int len);

protected:
    std::string name_;
};

// Out of line: it calls the virtuals above, so it needs the complete class.
inline void Port::raw(const uint8_t* msg, int len)
{
    if (len < 2) return;
    const int ch = msg[0] & 0x0F;
    switch (msg[0] & 0xF0) {
    case 0x80: noteOff(ch, msg[1] & 0x7F); break;
    case 0x90:
        if (len < 3) break;
        // Velocity 0 is a note-off by the running-status convention.
        if (msg[2] & 0x7F) noteOn(ch, msg[1] & 0x7F, msg[2] & 0x7F);
        else               noteOff(ch, msg[1] & 0x7F);
        break;
    case 0xB0: if (len >= 3) cc(ch, msg[1] & 0x7F, msg[2] & 0x7F); break;
    case 0xC0: programChange(ch, -1, -1, msg[1] & 0x7F); break;
    case 0xE0: if (len >= 3) pitchBend(ch, (msg[1] & 0x7F) | ((msg[2] & 0x7F) << 7)); break;
    default:   break;   // 0xA0, 0xD0: nothing typed to map them onto
    }
}
