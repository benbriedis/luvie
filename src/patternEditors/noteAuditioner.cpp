// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "noteAuditioner.hpp"
#include "port.hpp"
#include "portRegistry.hpp"
#include <FL/Fl.H>
#include <algorithm>

void NoteAuditioner::sendOff(const Pending* p)
{
    if (midiSink) { midiSink(p->portName, p->channel, p->pitch, 0, false); return; }
    if (portReg)
        if (Port* port = portReg->find(p->portName)) port->noteOff(p->channel, p->pitch);
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

    if (midiSink) {
        // Plugin mode: the host emits the note (no local PortRegistry).
        midiSink(r.portName, r.channel0, midi, velocity, true);
    } else {
        if (!portReg || r.portName.empty()) return;
        Port* port = portReg->find(r.portName);
        if (!port) return;
        port->noteOn(r.channel0, midi, velocity);
    }

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

    if (midiSink) {
        midiSink(r.portName, r.channel0, midi, velocity, true);
    } else {
        if (!portReg || r.portName.empty()) return;
        Port* port = portReg->find(r.portName);
        if (!port) return;
        port->noteOn(r.channel0, midi, velocity);
    }
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

void NoteAuditioner::offCb(void* data)
{
    auto* p    = static_cast<Pending*>(data);
    auto* self = p->self;
    self->sendOff(p);
    auto& v = self->pending;
    v.erase(std::remove(v.begin(), v.end(), p), v.end());
    delete p;
}
