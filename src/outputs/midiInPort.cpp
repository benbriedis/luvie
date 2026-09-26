// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "midiInPort.hpp"
#include "jackTransport.hpp"
#include "pluginPorts.hpp"
#include <RtMidi.h>
#include <cstdio>
#include <vector>

MidiInputManager::MidiInputManager()
{
    for (int i = 0; i < kMaxMidiInputs; i++) {
        slots_[i].mgr   = this;
        slots_[i].index = i;
    }
}

MidiInputManager::~MidiInputManager()
{
    // Sink first: once it reads null the RT thread stops touching this object, so
    // it cannot be mid-push into a ring while the rings are about to be destroyed.
    if (jack_) jack_->setMidiInSink(nullptr);
    for (auto& s : slots_) close(s);
}

// ── Producer side (RT / RtMidi threads) ───────────────────────────────────────

void MidiInputManager::pushFromRt(int slot, const uint8_t* data, int len)
{
    if (slot < 0 || slot >= kMaxMidiInputs || len < 1) return;
    if (len > 3) len = 3;   // we only forward channel-voice messages

    Slot& s = slots_[slot];
    const uint32_t head = s.head.load(std::memory_order_relaxed);
    const uint32_t tail = s.tail.load(std::memory_order_acquire);
    if (head - tail >= kRingSize) return;   // full: drop rather than block

    InEvent& e = s.ring[head & (kRingSize - 1)];
    for (int i = 0; i < len; i++) e.data[i] = data[i];
    e.len = static_cast<uint8_t>(len);

    // Release: the consumer must see the bytes above once it sees this index.
    s.head.store(head + 1, std::memory_order_release);
}

// The only thing a producer does beyond the ring push: one pipe write per batch,
// never one per event. claimWakeup() is what keeps it to one.
void MidiInputManager::postWakeup()
{
    if (awakeFn) awakeFn(&MidiInputManager::drainThunk, this);
    else wakePending_.store(false, std::memory_order_release);   // nobody to wake
}

bool MidiInputManager::claimWakeup()
{
    return !wakePending_.exchange(true, std::memory_order_acq_rel);
}

// ── Consumer side (UI thread) ─────────────────────────────────────────────────

void MidiInputManager::drain()
{
    // Cleared first: an event pushed between the loop ending and the flag being
    // cleared would otherwise never get a wakeup of its own.
    wakePending_.store(false, std::memory_order_release);

    for (auto& s : slots_) {
        for (;;) {
            const uint32_t tail = s.tail.load(std::memory_order_relaxed);
            if (tail == s.head.load(std::memory_order_acquire)) break;

            const InEvent& e = s.ring[tail & (kRingSize - 1)];
            uint8_t data[3] = { e.data[0], e.data[1], e.data[2] };
            const int len   = e.len;
            s.tail.store(tail + 1, std::memory_order_release);

            // An input closed since the event was queued no longer exists for
            // the instruments, so its leftovers are dropped here.
            if (sink_ && s.used) sink_(s.index, data, len);
        }
    }
}

void MidiInputManager::injectFromHost(int lv2Index, const uint8_t* data, int len)
{
    if (len < 1 || !sink_) return;
    if (len > 3) len = 3;
    // The first Plugin input mapped to this LV2 input owns it. Several can share
    // the last one on overflow, but only one of them can receive.
    for (const auto& in : cfg_) {
        if (in.backend != MidiBackend::Plugin) continue;
        if (pluginInputIndex(cfg_, in.name) != lv2Index) continue;
        const int slot = slotForName(in.name);
        if (slot >= 0) sink_(slot, data, len);
        return;
    }
}

int MidiInputManager::slotForName(const std::string& name) const
{
    for (const auto& s : slots_)
        if (s.used && s.name == name) return s.index;
    return -1;
}

// ── Backend lifecycle (UI thread) ─────────────────────────────────────────────

void MidiInputManager::apply(const std::vector<MidiInputPort>& ins, JackTransport* jack)
{
    const bool jackChanged = (jack != jack_);
    if (jackChanged && jack_) jack_->setMidiInSink(nullptr);

    auto wanted = [&](const Slot& s) -> const MidiInputPort* {
        for (const auto& in : ins)
            if (in.name == s.name) return &in;
        return nullptr;
    };

    // Close what has gone, and what has changed backend or JACK client. The
    // closes use the old client, so they come before jack_ moves on.
    for (auto& s : slots_) {
        if (!s.used) continue;
        const MidiInputPort* in = wanted(s);
        if (!in) {
            close(s);
            s.used = false;
            s.name.clear();
        } else if (in->backend != s.backend
                   || (jackChanged && s.backend == MidiBackend::Jack)) {
            close(s);
            s.backend = in->backend;
        }
    }
    jack_ = jack;

    // Open anything new, and anything closed above that still exists.
    for (const auto& in : ins) {
        int idx = slotForName(in.name);
        if (idx < 0) {
            for (auto& s : slots_)
                if (!s.used) { idx = s.index; break; }
            if (idx < 0) {
                fprintf(stderr, "MidiInputManager: more than %d inputs; '%s' ignored\n",
                        kMaxMidiInputs, in.name.c_str());
                continue;
            }
            Slot& s = slots_[idx];
            // Nothing should be left in a free slot's ring, but a stale event
            // must not be credited to the input that takes the slot over.
            s.tail.store(s.head.load(std::memory_order_acquire), std::memory_order_release);
            s.used    = true;
            s.name    = in.name;
            s.backend = in.backend;
        }
        if (!slots_[idx].open) open(slots_[idx]);
    }
    cfg_ = ins;
    updateJackSink();
}

void MidiInputManager::rename(const std::string& oldName, const std::string& newName)
{
    const int idx = slotForName(oldName);
    if (idx < 0 || oldName == newName) return;
    Slot& s = slots_[idx];
    for (auto& in : cfg_)
        if (in.name == oldName) in.name = newName;

    if (s.backend == MidiBackend::Jack && s.open && jack_) {
        s.name = newName;
        if (!jack_->renameMidiInPort(idx, newName)) {
            close(s);
            open(s);
        }
    } else if (s.backend == MidiBackend::Native && s.open) {
        close(s);
        s.name = newName;
        open(s);
    } else {
        s.name = newName;
    }
    updateJackSink();
}

void MidiInputManager::reregisterJack()
{
    if (!jack_) return;
    for (auto& s : slots_)
        if (s.used && s.backend == MidiBackend::Jack)
            s.open = openJack(s);
    updateJackSink();
}

void MidiInputManager::open(Slot& s)
{
    switch (s.backend) {
        case MidiBackend::Jack:   s.open = openJack(s);   break;
        case MidiBackend::Native: s.open = openNative(s); break;
        // Hosted, the host owns the connection and feeds injectFromHost(); Debug
        // is an output-only sink and never reaches here. Neither opens anything.
        case MidiBackend::Plugin:
        case MidiBackend::Debug:  s.open = false;         break;
    }
}

bool MidiInputManager::openJack(Slot& s)
{
    if (!jack_) return false;   // no client yet; reregisterJack() retries later
    return jack_->addMidiInPort(s.index, s.name);
}

bool MidiInputManager::openNative(Slot& s)
{
    try {
        s.in = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "luvie");
        s.in->openVirtualPort(s.name);
        // Sysex, timing clock and active sensing are all noise to us, and sysex in
        // particular would arrive as a message too long for the ring.
        s.in->ignoreTypes(true, true, true);
        s.in->setCallback(&MidiInputManager::rtMidiCallback, &s);
    } catch (const RtMidiError& e) {
        fprintf(stderr, "MidiInputManager: could not open '%s': %s\n",
                s.name.c_str(), e.getMessage().c_str());
        s.in.reset();
        return false;
    }
    return true;
}

void MidiInputManager::close(Slot& s)
{
    if (s.in) {
        try { s.in->cancelCallback(); } catch (const RtMidiError&) {}
        s.in.reset();   // the destructor closes the virtual port
    }
    if (jack_ && s.backend == MidiBackend::Jack && s.open)
        jack_->removeMidiInPort(s.index);
    s.open = false;
}

void MidiInputManager::updateJackSink()
{
    if (!jack_) return;
    bool anyJack = false;
    for (const auto& s : slots_)
        if (s.used && s.open && s.backend == MidiBackend::Jack) anyJack = true;
    jack_->setMidiInSink(anyJack ? this : nullptr);
}

// RtMidi's own thread, one per input. Not the audio thread, but still not the UI
// thread, so it goes through the same ring as the JACK path rather than touching
// FLTK.
void MidiInputManager::rtMidiCallback(double, std::vector<unsigned char>* msg, void* user)
{
    auto* s = static_cast<Slot*>(user);
    if (!s || !msg || msg->empty()) return;
    s->mgr->pushFromRt(s->index, msg->data(), static_cast<int>(msg->size()));
    if (s->mgr->claimWakeup()) s->mgr->postWakeup();
}
