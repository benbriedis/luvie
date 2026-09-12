// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "midiBackend.hpp"
#include "timelineIO.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class JackTransport;
class RtMidiIn;

// The project's single MIDI input. Owns whichever backend resource the settings
// ask for and hands the events it receives to one sink on the UI thread.
//
// This is the input counterpart of PortRegistry: the owner calls apply() whenever
// the settings change and the manager makes the live resource match, creating,
// destroying or swapping as needed. It is not a Port subclass — Port is an
// output-only interface (noteOn/cc/programChange), and nothing in it fits here.
//
// Threading. Events arrive on a thread we do not own: JACK's RT process thread for
// Jack, RtMidi's ALSA thread for Native. Neither may touch FLTK or allocate, so
// both push into the lock-free ring below and post a wakeup; the sink is then
// called on the UI thread when that wakeup is serviced. In plugin mode nothing is
// opened at all — the host owns the connection and the LV2 UI feeds the sink
// directly from its port_event.
class MidiInputManager {
public:
    MidiInputManager();
    ~MidiInputManager();

    MidiInputManager(const MidiInputManager&)            = delete;
    MidiInputManager& operator=(const MidiInputManager&) = delete;

    // A received message, already filtered to the configured channel. Called on
    // the UI thread. len is 1..3.
    using Sink = std::function<void(const uint8_t* data, int len)>;
    void setSink(Sink s) { sink_ = std::move(s); }

    // Make the live input match `cfg`. Reopens only when the backend actually
    // changes; a channel-only change just updates the filter. `jack` may be null
    // (no JACK client yet), in which case a Jack input is simply not opened —
    // reregisterJack() picks it up when a client appears.
    void apply(const MidiInput& cfg, JackTransport* jack);

    // Re-register the Jack input port on a freshly (re)opened JACK client, as
    // PortRegistry::reregisterJack() does for the outputs.
    void reregisterJack();

    // Feeds the sink directly, bypassing the ring. For LV2 plugin mode, where the
    // event arrives on the UI thread already (LuvieUI::port_event) and there is
    // no foreign thread to marshal from. Applies the channel filter.
    void injectFromHost(const uint8_t* data, int len);

    // The name the Jack and Native ports are registered under. There is one input
    // and no name field for it, so it is fixed.
    static constexpr const char* kPortName = "midi_in";

    // ── Producer side: called on the JACK RT thread / RtMidi's thread ─────────
    // Copies one message into the ring. Allocation-free, lock-free and
    // syscall-free, so it is safe to call from the RT thread. Silently drops the
    // event if the ring is full — dropping a note beats blocking the audio thread.
    void pushFromRt(const uint8_t* data, int len);
    // True if anything was pushed since the last wakeup was posted, and claims the
    // right to post one. Lets a producer coalesce a whole cycle's events into a
    // single Fl::awake, rather than one per event.
    bool claimWakeup();
    // Asks the UI thread to drain, via awakeFn. Call only after claimWakeup()
    // returned true, so a burst of events costs one wakeup and not one each.
    void postWakeup();

    // Drains the ring into the sink. Call on the UI thread only.
    void drain();

    // Marshals drain() onto the owner's UI thread (wraps Fl::awake), exactly as
    // JackTransport::awakeFn does for the transport's cross-thread callbacks. Left
    // null by the headless DSP and the tests, where events simply queue until
    // something calls drain() itself.
    void (*awakeFn)(void (*)(void*), void*) = nullptr;

private:
    void closeAll();
    bool openJack(JackTransport* jack);
    bool openNative();
    // True if the message is on the configured channel (or it is not a channel
    // message, e.g. a system message, which is never filtered out).
    bool passesFilter(const uint8_t* data, int len) const;

    static void rtMidiCallback(double, std::vector<unsigned char>*, void*);
    static void drainThunk(void* self) { static_cast<MidiInputManager*>(self)->drain(); }

    Sink           sink_;
    MidiBackend    backend_ = MidiBackend::Jack;
    int            channel_ = 0;        // 0 = Any; 1-16 = that channel
    bool           open_    = false;    // is a backend resource currently open?
    JackTransport* jack_    = nullptr;

    std::unique_ptr<RtMidiIn> in_;      // Native only; null otherwise

    // ── Lock-free SPSC ring ──────────────────────────────────────────────────
    // Fixed capacity, allocated once with the manager, so a push is a couple of
    // stores and one atomic increment. Single producer (one backend is open at a
    // time) and single consumer (the UI thread).
    struct InEvent { uint8_t data[3]; uint8_t len; };
    static constexpr uint32_t kRingSize = 1024;   // power of two; mask instead of %
    InEvent               ring_[kRingSize];
    std::atomic<uint32_t> head_{0};   // producer writes
    std::atomic<uint32_t> tail_{0};   // consumer writes
    std::atomic<bool>     wakePending_{false};
};
