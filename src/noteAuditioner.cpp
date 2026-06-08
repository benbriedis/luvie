#include "noteAuditioner.hpp"
#include "port.hpp"
#include "portRegistry.hpp"
#include <FL/Fl.H>
#include <algorithm>

NoteAuditioner::~NoteAuditioner()
{
    for (Pending* p : pending) {
        Fl::remove_timeout(offCb, p);
        if (portReg)
            if (Port* port = portReg->find(p->portName)) port->noteOff(p->channel, p->pitch);
        delete p;
    }
}

void NoteAuditioner::play(int instrumentId, int midi, int velocity, float seconds)
{
    if (!portReg || !instrRoute || midi < 0 || midi > 127) return;
    MidiInstrRoute r = instrRoute(instrumentId);
    if (r.portName.empty()) return;
    Port* port = portReg->find(r.portName);
    if (!port) return;

    velocity = std::clamp(velocity, 1, 127);
    port->noteOn(r.channel0, midi, velocity);

    auto* p = new Pending{this, r.portName, r.channel0, midi};
    pending.push_back(p);
    Fl::add_timeout(seconds, offCb, p);
}

void NoteAuditioner::offCb(void* data)
{
    auto* p    = static_cast<Pending*>(data);
    auto* self = p->self;
    if (self->portReg)
        if (Port* port = self->portReg->find(p->portName)) port->noteOff(p->channel, p->pitch);
    auto& v = self->pending;
    v.erase(std::remove(v.begin(), v.end(), p), v.end());
    delete p;
}
