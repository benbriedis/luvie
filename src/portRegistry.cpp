#include "portRegistry.hpp"
#include "jackPort.hpp"
#include "rtMidiPort.hpp"
#include "debugPort.hpp"
#include "jackTransport.hpp"
#include <algorithm>
#include <utility>

std::unique_ptr<Port> PortRegistry::make(const JackOutput& o)
{
    switch (o.backend) {
        case MidiBackend::Native:
            return std::make_unique<RtMidiPort>(o.portName);
        case MidiBackend::Debug: {
            auto p = std::make_unique<DebugPort>(o.portName);
            p->pitchName = debugPitchName;
            return p;
        }
        case MidiBackend::Jack:
            break;
    }
    return std::make_unique<JackPort>(jack_, o.portName);
}

void PortRegistry::reconcile(const std::vector<JackOutput>& ports)
{
    std::vector<std::unique_ptr<Port>> next;
    next.reserve(ports.size());

    for (const auto& o : ports) {
        // Reuse an existing port with the same name AND backend.
        auto it = std::find_if(ports_.begin(), ports_.end(), [&](const std::unique_ptr<Port>& p) {
            return p && p->name() == o.portName && p->backend() == o.backend;
        });
        if (it != ports_.end()) {
            next.push_back(std::move(*it));
            ports_.erase(it);
        } else {
            next.push_back(make(o));
        }
    }
    // Whatever is left in ports_ was removed or had its backend changed; destroying
    // the unique_ptrs tears down the underlying JACK/ALSA resource.
    ports_ = std::move(next);
}

void PortRegistry::rename(const std::string& oldName, const std::string& newName)
{
    if (Port* p = find(oldName)) p->rename(newName);
}

void PortRegistry::reregisterJack()
{
    if (!jack_) return;
    for (auto& p : ports_)
        if (p && p->backend() == MidiBackend::Jack)
            jack_->addMidiPort(p->name());
}

Port* PortRegistry::find(const std::string& name) const
{
    for (const auto& p : ports_)
        if (p && p->name() == name) return p.get();
    return nullptr;
}

bool PortRegistry::anyJack() const
{
    for (const auto& p : ports_)
        if (p && p->backend() == MidiBackend::Jack) return true;
    return false;
}

bool PortRegistry::anySoft() const
{
    for (const auto& p : ports_)
        if (p && p->softSequenced()) return true;
    return false;
}
