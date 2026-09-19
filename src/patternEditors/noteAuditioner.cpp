// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "noteAuditioner.hpp"
#include "port.hpp"
#include "portRegistry.hpp"
#include <FL/Fl.H>
#include <algorithm>

void NoteAuditioner::sendNote(const std::string& portName, int ch, int midi, int velocity, bool on)
{
    if (midiSink) {
        const uint8_t m[3] = { static_cast<uint8_t>((on ? 0x90 : 0x80) | (ch & 0x0F)),
                               static_cast<uint8_t>(midi & 0x7F),
                               static_cast<uint8_t>(on ? (velocity & 0x7F) : 0) };
        midiSink(portName, m, 3);
        return;
    }
    if (!portReg || portName.empty()) return;
    Port* port = portReg->find(portName);
    if (!port) return;
    if (on) port->noteOn(ch, midi, velocity);
    else    port->noteOff(ch, midi);
}

void NoteAuditioner::sendOff(const Pending* p)
{
    sendNote(p->portName, p->channel, p->pitch, 0, false);
}

NoteAuditioner::~NoteAuditioner()
{
    for (Pending* p : pending) {
        Fl::remove_timeout(offCb, p);
        sendOff(p);
        delete p;
    }
    // Held notes have no timeout to cancel, but they must not be left sounding.
    for (Pending* p : heldNotes) {
        sendOff(p);
        delete p;
    }
}

void NoteAuditioner::play(int instrumentId, int midi, int velocity, float seconds)
{
    if (!instrRoute || midi < 0 || midi > 127) return;
    MidiInstrRoute r = instrRoute(instrumentId);
    velocity = std::clamp(velocity, 1, 127);

    // Plugin mode has no local PortRegistry: sendNote hands it to the host.
    if (!midiSink && (!portReg || !portReg->find(r.portName))) return;
    sendNote(r.portName, r.channel0, midi, velocity, true);

    auto* p = new Pending{this, r.portName, r.channel0, midi};
    pending.push_back(p);
    Fl::add_timeout(seconds, offCb, p);
}

void NoteAuditioner::noteOn(int instrumentId, int midi, int velocity)
{
    if (!instrRoute || midi < 0 || midi > 127) return;
    // A second note-on for a pitch already sounding (a stuck key, or two sources
    // playing it) would otherwise leave the first one hanging: release it first.
    noteOff(instrumentId, midi);

    MidiInstrRoute r = instrRoute(instrumentId);
    velocity = std::clamp(velocity, 1, 127);

    if (!midiSink && (!portReg || !portReg->find(r.portName))) return;
    sendNote(r.portName, r.channel0, midi, velocity, true);
    heldNotes.push_back(new Pending{this, r.portName, r.channel0, midi});
}

void NoteAuditioner::noteOff(int instrumentId, int midi)
{
    (void)instrumentId;   // the route was captured at note-on and is what must be used
    for (int i = (int)heldNotes.size() - 1; i >= 0; i--) {
        Pending* p = heldNotes[i];
        if (p->pitch != midi) continue;
        sendOff(p);
        heldNotes.erase(heldNotes.begin() + i);
        delete p;
        return;
    }
}

void NoteAuditioner::param(int instrumentId, int ccNumber, int value)
{
    if (!instrRoute) return;
    MidiInstrRoute r  = instrRoute(instrumentId);
    const int      ch = r.channel0 & 0x0F;
    if (midiSink) {
        uint8_t m[3];
        if (ccNumber < 0) {
            value = std::clamp(value, 0, 16383);
            m[0] = static_cast<uint8_t>(0xE0 | ch);
            m[1] = static_cast<uint8_t>(value & 0x7F);
            m[2] = static_cast<uint8_t>((value >> 7) & 0x7F);
        } else {
            m[0] = static_cast<uint8_t>(0xB0 | ch);
            m[1] = static_cast<uint8_t>(ccNumber & 0x7F);
            m[2] = static_cast<uint8_t>(std::clamp(value, 0, 127));
        }
        midiSink(r.portName, m, 3);
        return;
    }
    if (!portReg || r.portName.empty()) return;
    Port* port = portReg->find(r.portName);
    if (!port) return;
    if (ccNumber < 0) port->pitchBend(ch, std::clamp(value, 0, 16383));
    else              port->cc(ch, ccNumber, std::clamp(value, 0, 127));
}

void NoteAuditioner::offCb(void* data)
{
    auto* p    = static_cast<Pending*>(data);
    auto* self = p->self;
    self->sendOff(p);
    auto& v = self->pending;
    v.erase(std::remove(v.begin(), v.end(), p), v.end());
    delete p;
}
