// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef NOTE_AUDITIONER_HPP
#define NOTE_AUDITIONER_HPP

#include "playhead.hpp"   // MidiInstrRoute
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class PortRegistry;

// Plays one-off audition notes — e.g. clicking a row label in a pattern editor.
// Sends note-on immediately and schedules the matching note-off after a wall-clock
// duration via an FLTK timeout. Routes through the same PortRegistry + instrument
// routing as the song playhead, so a click sounds on the selected instrument's port.
class NoteAuditioner {
public:
    ~NoteAuditioner();

    void setPortRegistry(PortRegistry* r)                  { portReg = r; }
    void setInstrRoute(std::function<MidiInstrRoute(int)> r) { instrRoute = std::move(r); }

    // Alternative sink used when there is no local PortRegistry (LV2 plugin mode):
    // the message is emitted by the host instead, e.g. forwarded to the DSP's MIDI
    // out. (portName, bytes, len) — a complete channel message, channel included.
    // The port name is passed through so the sink can pick the right output,
    // exactly as the PortRegistry path does.
    using MidiSink = std::function<void(const std::string&, const uint8_t*, int)>;
    void setMidiSink(MidiSink s) { midiSink = std::move(s); }

    // Note-on to the instrument's port now; note-off after `seconds`.
    void play(int instrumentId, int midi, int velocity, float seconds);

    // Held notes, for MIDI input: the length is the player's, not a timeout's, so
    // these come in pairs rather than as one timed play(). A noteOn without its
    // noteOff is released by the destructor, as a timed one would be.
    void noteOn (int instrumentId, int midi, int velocity);
    void noteOff(int instrumentId, int midi);

    // A controller value, sent straight through — how a bound MIDI-learn control is
    // heard as it moves. ccNumber < 0 means pitch bend (value 0-16383), following
    // ccForType(); otherwise a CC with value 0-127.
    void param(int instrumentId, int ccNumber, int value);

    // A message from the MIDI input forwarded verbatim to the instrument's port,
    // with only its channel rewritten to the route's. This is what controls Luvie
    // has no binding for take: unlike param(), the CC number is not remapped
    // through ccForType(), so the synth sees what the controller actually sent and
    // its own MIDI learn can bind it. len is 1..3.
    void passThrough(int instrumentId, const uint8_t* msg, int len);

private:
    struct Pending { NoteAuditioner* self; std::string portName; int channel; int pitch; };
    static void offCb(void* data);
    void        sendOff(const Pending* p);
    void        sendNote(const std::string& portName, int ch, int midi, int velocity, bool on);

    PortRegistry*                      portReg = nullptr;
    std::function<MidiInstrRoute(int)> instrRoute;
    MidiSink                           midiSink;   // plugin-mode emission
    std::vector<Pending*>              pending;   // outstanding timeout payloads
    // Notes turned on by noteOn() and not yet turned off. Held separately from
    // `pending` because no timeout owns them — only a matching noteOff (or the
    // destructor) ends them.
    std::vector<Pending*>              heldNotes;
};

#endif
