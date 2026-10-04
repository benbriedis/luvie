// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "noteAuditioner.hpp"
#include "paramDefs.hpp"
#include "luvieDebug.hpp"
#include "port.hpp"
#include "portRegistry.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <cstdio>

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
    heldNotes.push_back(new Pending{this, r.portName, r.channel0, midi, instrumentId});
}

void NoteAuditioner::noteOff(int instrumentId, int midi)
{
    // Matched by instrument, not just pitch: one key can be sounding on several
    // instruments at once, and each is released on its own. The route to send the
    // note-off on is the one captured at note-on.
    for (int i = (int)heldNotes.size() - 1; i >= 0; i--) {
        Pending* p = heldNotes[i];
        if (p->pitch != midi || p->instrumentId != instrumentId) continue;
        sendOff(p);
        heldNotes.erase(heldNotes.begin() + i);
        delete p;
        return;
    }
}

void NoteAuditioner::param(int instrumentId, int outCode, int value)
{
    if (!instrRoute) return;
    MidiInstrRoute r = instrRoute(instrumentId);
    uint8_t m[3];
    const int len = encodeParamMessage(outCode, r.channel0, value, m);
    if (midiSink) {
        midiSink(r.portName, m, len);
        return;
    }
    if (!portReg || r.portName.empty()) return;
    Port* port = portReg->find(r.portName);
    if (!port) return;
    port->raw(m, len);
}

void NoteAuditioner::passThrough(int instrumentId, const uint8_t* msg, int len)
{
    if (!instrRoute || len < 1 || len > 3) {
        if (luvieDebug())
            fprintf(stderr, "[luvie] passthru: dropped (%s)\n",
                    instrRoute ? "bad length" : "no instrument routing");
        return;
    }
    MidiInstrRoute r = instrRoute(instrumentId);

    uint8_t m[3] = {};
    for (int i = 0; i < len; i++) m[i] = msg[i];
    // The status byte's channel is the one thing that is not passed through: the
    // controller's channel is whatever it happens to be set to, and the instrument's
    // is where this has to land, exactly as param() and sendNote() do.
    m[0] = static_cast<uint8_t>((msg[0] & 0xF0) | (r.channel0 & 0x0F));

    if (luvieDebug())
        fprintf(stderr, "[luvie] passthru: %02X %02X %02X -> instrument %d, "
                        "port \"%s\" ch %d (%s)\n",
                m[0], len > 1 ? m[1] : 0, len > 2 ? m[2] : 0, instrumentId,
                r.portName.c_str(), r.channel0 + 1,
                midiSink ? "plugin sink" : "port registry");

    if (midiSink) { midiSink(r.portName, m, len); return; }
    if (!portReg || r.portName.empty()) return;
    Port* port = portReg->find(r.portName);
    if (!port) {
        if (luvieDebug())
            fprintf(stderr, "[luvie] passthru: no open port named \"%s\"\n",
                    r.portName.c_str());
        return;
    }
    port->raw(m, len);
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
