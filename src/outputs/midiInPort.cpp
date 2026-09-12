// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "midiInPort.hpp"
#include "jackTransport.hpp"
#include <RtMidi.h>
#include <cstdio>
#include <vector>

MidiInputManager::MidiInputManager()  = default;
MidiInputManager::~MidiInputManager() { closeAll(); }

// ── Producer side (RT / RtMidi thread) ────────────────────────────────────────

void MidiInputManager::pushFromRt(const uint8_t* data, int len)
{
    if (len < 1) return;
    if (len > 3) len = 3;   // we only forward channel-voice messages

    const uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    if (head - tail >= kRingSize) return;   // full: drop rather than block

    InEvent& e = ring_[head & (kRingSize - 1)];
    for (int i = 0; i < len; i++) e.data[i] = data[i];
    e.len = static_cast<uint8_t>(len);

    // Release: the consumer must see the bytes above once it sees this index.
    head_.store(head + 1, std::memory_order_release);
}

// The only thing the producer does beyond the ring push: one pipe write per
// batch, never one per event. claimWakeup() is what keeps it to one.
void MidiInputManager::postWakeup()
{
    if (awakeFn) awakeFn(&MidiInputManager::drainThunk, this);
    else wakePending_.store(false, std::memory_order_release);   // nobody to wake
}

bool MidiInputManager::claimWakeup()
{
    if (head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed))
        return false;                                     // nothing to wake for
    return !wakePending_.exchange(true, std::memory_order_acq_rel);
}

// ── Consumer side (UI thread) ─────────────────────────────────────────────────

void MidiInputManager::drain()
{
    // Cleared first: an event pushed between the loop ending and the flag being
    // cleared would otherwise never get a wakeup of its own.
    wakePending_.store(false, std::memory_order_release);

    for (;;) {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) break;

        const InEvent& e = ring_[tail & (kRingSize - 1)];
        uint8_t data[3] = { e.data[0], e.data[1], e.data[2] };
        const int len   = e.len;
        tail_.store(tail + 1, std::memory_order_release);

        if (sink_ && passesFilter(data, len)) sink_(data, len);
    }
}

void MidiInputManager::injectFromHost(const uint8_t* data, int len)
{
    if (len < 1) return;
    if (len > 3) len = 3;
    if (sink_ && passesFilter(data, len)) sink_(data, len);
}

bool MidiInputManager::passesFilter(const uint8_t* data, int len) const
{
    if (channel_ == 0 || len < 1) return true;          // "Any"
    const uint8_t status = data[0];
    if (status < 0x80 || status >= 0xF0) return true;   // not a channel message
    return (status & 0x0F) == (channel_ - 1);
}

// ── Backend lifecycle (UI thread) ─────────────────────────────────────────────

void MidiInputManager::apply(const MidiInput& cfg, JackTransport* jack)
{
    const bool backendChanged = (cfg.backend != backend_) || (jack != jack_);
    channel_ = cfg.channel;          // a channel-only change just moves the filter
    if (!backendChanged && open_) return;

    closeAll();
    backend_ = cfg.backend;
    jack_    = jack;

    switch (backend_) {
        case MidiBackend::Jack:   open_ = openJack(jack); break;
        case MidiBackend::Native: open_ = openNative();   break;
        // Hosted, the host owns the connection and feeds injectFromHost(); Debug
        // is an output-only sink and never reaches here. Neither opens anything.
        case MidiBackend::Plugin:
        case MidiBackend::Debug:  open_ = false;          break;
    }
}

void MidiInputManager::reregisterJack()
{
    if (backend_ != MidiBackend::Jack || !jack_) return;
    open_ = openJack(jack_);
}

bool MidiInputManager::openJack(JackTransport* jack)
{
    if (!jack) return false;   // no client yet; reregisterJack() retries later
    jack->setMidiInSink(this);
    return jack->addMidiInPort(kPortName);
}

bool MidiInputManager::openNative()
{
    try {
        in_ = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "luvie");
        in_->openVirtualPort(kPortName);
        // Sysex, timing clock and active sensing are all noise to us, and sysex in
        // particular would arrive as a message too long for the ring.
        in_->ignoreTypes(true, true, true);
        in_->setCallback(&MidiInputManager::rtMidiCallback, this);
    } catch (const RtMidiError& e) {
        fprintf(stderr, "MidiInputManager: could not open '%s': %s\n",
                kPortName, e.getMessage().c_str());
        in_.reset();
        return false;
    }
    return true;
}

void MidiInputManager::closeAll()
{
    if (in_) {
        try { in_->cancelCallback(); } catch (const RtMidiError&) {}
        in_.reset();   // the destructor closes the virtual port
    }
    if (jack_ && backend_ == MidiBackend::Jack) {
        // Sink first: once it reads null the RT thread stops touching this object,
        // so it cannot be mid-push into the ring while the port goes away (or,
        // from the destructor, while the ring itself is about to be destroyed).
        jack_->setMidiInSink(nullptr);
        jack_->removeMidiInPort();
    }
    open_ = false;
}

// RtMidi's own thread. Not the audio thread, but still not the UI thread, so it
// goes through the same ring as the JACK path rather than touching FLTK.
void MidiInputManager::rtMidiCallback(double, std::vector<unsigned char>* msg, void* user)
{
    auto* self = static_cast<MidiInputManager*>(user);
    if (!self || !msg || msg->empty()) return;
    self->pushFromRt(msg->data(), static_cast<int>(msg->size()));
    if (self->claimWakeup()) self->postWakeup();
}
