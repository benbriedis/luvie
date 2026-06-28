#include "noteAuditioner.hpp"
#include "port.hpp"
#include "portRegistry.hpp"
#include <FL/Fl.H>
#include <algorithm>

void NoteAuditioner::sendOff(const Pending* p)
{
    if (midiSink) { midiSink(p->channel, p->pitch, 0, false); return; }
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
}

void NoteAuditioner::play(int instrumentId, int midi, int velocity, float seconds)
{
    if (!instrRoute || midi < 0 || midi > 127) return;
    MidiInstrRoute r = instrRoute(instrumentId);
    velocity = std::clamp(velocity, 1, 127);

    if (midiSink) {
        // Plugin mode: the host emits the note (no local PortRegistry).
        midiSink(r.channel0, midi, velocity, true);
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

void NoteAuditioner::offCb(void* data)
{
    auto* p    = static_cast<Pending*>(data);
    auto* self = p->self;
    self->sendOff(p);
    auto& v = self->pending;
    v.erase(std::remove(v.begin(), v.end(), p), v.end());
    delete p;
}
