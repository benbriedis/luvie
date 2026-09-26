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
#include <vector>

class JackTransport;
class RtMidiIn;

// The project's MIDI inputs. Owns whichever backend resource each input asks for
// and hands the events they receive to one sink on the UI thread, tagged with the
// slot they arrived on.
//
// This is the input counterpart of PortRegistry: the owner calls apply() whenever
// the settings change and the manager makes the live resources match, creating,
// destroying or swapping as needed. It is not a Port subclass — Port is an
// output-only interface (noteOn/cc/programChange), and nothing in it fits here.
//
// Each input occupies a slot, and keeps it for as long as it exists, so a slot
// index is a stable handle for "this input" (JACK's port table and the sink both
// use it). Slots are fixed — kMaxMidiInputs of them, allocated with the manager.
//
// Threading. Events arrive on threads we do not own: JACK's RT process thread for
// Jack, RtMidi's ALSA thread (one per input) for Native. None may touch FLTK or
// allocate, so each pushes into its slot's lock-free ring — one ring per slot, so
// every ring has a single producer — and posts a wakeup; the sink is then called
// on the UI thread when that wakeup is serviced. In plugin mode nothing is opened
// at all — the host owns the connections and the LV2 UI feeds the sink directly
// from its port_event.
class MidiInputManager {
public:
    MidiInputManager();
    ~MidiInputManager();

    MidiInputManager(const MidiInputManager&)            = delete;
    MidiInputManager& operator=(const MidiInputManager&) = delete;

    // A received message and the slot it came in on. Called on the UI thread.
    // len is 1..3. Not filtered by channel: each instrument says which channel it
    // listens to, and the owner filters.
    using Sink = std::function<void(int slot, const uint8_t* data, int len)>;
    void setSink(Sink s) { sink_ = std::move(s); }

    // Make the live inputs match `ins` (by name + backend). Inputs no longer
    // listed are closed, new ones opened, and ones whose backend changed are
    // reopened. `jack` may be null (no JACK client yet), in which case Jack
    // inputs are simply not opened — reregisterJack() picks them up when a client
    // appears. Inputs past kMaxMidiInputs are ignored.
    void apply(const std::vector<MidiInputPort>& ins, JackTransport* jack);

    // Rename in place, so a Jack input keeps its connections. A Native input is
    // reopened under the new name: RtMidi cannot rename a virtual port.
    void rename(const std::string& oldName, const std::string& newName);

    // Re-register the Jack inputs on a freshly (re)opened JACK client, as
    // PortRegistry::reregisterJack() does for the outputs.
    void reregisterJack();

    // The slot holding input `name`, or -1.
    int slotForName(const std::string& name) const;

    // Feeds the sink directly, bypassing the rings. For LV2 plugin mode, where the
    // event arrives on the UI thread already (LuvieUI::port_event) and there is no
    // foreign thread to marshal from. `lv2Index` is the plugin's MIDI input it
    // arrived on, which is mapped back to the Plugin input that owns it.
    void injectFromHost(int lv2Index, const uint8_t* data, int len);

    // ── Producer side: called on the JACK RT thread / RtMidi's threads ────────
    // Copies one message into the slot's ring. Allocation-free, lock-free and
    // syscall-free, so it is safe to call from the RT thread. Silently drops the
    // event if the ring is full — dropping a note beats blocking the audio thread.
    void pushFromRt(int slot, const uint8_t* data, int len);
    // Claims the right to post a wakeup, false if one is already pending. Lets the
    // producers coalesce a whole cycle's events, on every input, into a single
    // Fl::awake rather than one per event.
    bool claimWakeup();
    // Asks the UI thread to drain, via awakeFn. Call only after claimWakeup()
    // returned true.
    void postWakeup();

    // Drains every ring into the sink. Call on the UI thread only.
    void drain();

    // Marshals drain() onto the owner's UI thread (wraps Fl::awake), exactly as
    // JackTransport::awakeFn does for the transport's cross-thread callbacks. Left
    // null by the headless DSP and the tests, where events simply queue until
    // something calls drain() itself.
    void (*awakeFn)(void (*)(void*), void*) = nullptr;

private:
    // ── Lock-free SPSC ring, one per slot ─────────────────────────────────────
    // Fixed capacity, allocated once with the manager, so a push is a couple of
    // stores and one atomic increment.
    struct InEvent { uint8_t data[3]; uint8_t len; };
    static constexpr uint32_t kRingSize = 512;   // power of two; mask instead of %

    struct Slot {
        // UI-thread state.
        bool        used    = false;   // holds an input
        bool        open    = false;   // a backend resource is currently open
        std::string name;
        MidiBackend backend = MidiBackend::Jack;
        std::unique_ptr<RtMidiIn> in;  // Native only; null otherwise
        // Set once by the manager's ctor; RtMidi's callback gets the Slot, and
        // needs these to reach the ring's owner and its own index.
        MidiInputManager* mgr   = nullptr;
        int               index = 0;

        // Ring: producer writes head, consumer (UI thread) writes tail.
        InEvent               ring[kRingSize];
        std::atomic<uint32_t> head{0};
        std::atomic<uint32_t> tail{0};
    };

    void open (Slot& s);
    void close(Slot& s);
    bool openJack  (Slot& s);
    bool openNative(Slot& s);
    // Keeps JACK's sink pointer set exactly while some Jack input is open, so the
    // RT thread reads no input at all when none is.
    void updateJackSink();

    static void rtMidiCallback(double, std::vector<unsigned char>*, void*);
    static void drainThunk(void* self) { static_cast<MidiInputManager*>(self)->drain(); }

    Sink           sink_;
    JackTransport* jack_ = nullptr;
    std::vector<MidiInputPort> cfg_;   // as last applied, in the user's order

    Slot                  slots_[kMaxMidiInputs];
    std::atomic<bool>     wakePending_{false};
};
