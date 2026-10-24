// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef CYCLE_DRIVER_HPP
#define CYCLE_DRIVER_HPP

#include "sequencer.hpp"
#include <atomic>
#include <cstdint>
#include <memory>

/*
 * The RT half every output backend shares: how one cycle of the backend's clock is
 * turned into a sorted batch of frame-stamped MIDI. The Sequencer decides *what*
 * plays; this decides how a window of the clock reaches it and how what it emits
 * lands on frames. JackTransport, InternalTransport and the LV2 DSP each used to
 * carry their own copy of this, and the copies drifted — one remembered a relocate
 * across a skipped cycle, two forgot it — so it lives here once.
 *
 * A backend reads its own clock into a ClockSample, calls runCycle(), and then
 * writes `events` (sorted by port, then frame) to wherever its MIDI goes. Messages
 * for ports it keeps no per-cycle buffer for are handed to emitUnbuffered() instead,
 * already stamped with their frame.
 */

// One cycle of a backend's clock.
struct ClockSample {
    int64_t frame     = 0;      // first frame of the window
    int64_t nframes   = 0;      // its length
    bool    rolling   = false;
    bool    relocated = false;  // the clock jumped since the previous cycle
};

// Counters for LUVIE_DEBUG and the tests. Written on the RT thread; relaxed, so a
// reader on another thread sees them a little late, which is all a report needs.
struct RtStats {
    std::atomic<uint32_t> cycles{0};
    std::atomic<uint32_t> relocates{0};    // clock jumps the engine acted on
    std::atomic<uint32_t> clamped{0};      // events whose frame fell outside the window
    std::atomic<uint32_t> dropped{0};      // events lost to a full buffer

    static void bump(std::atomic<uint32_t>& c) {
        c.store(c.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }
};

/*
 * A cycle's MIDI, fixed capacity. Sized once off the RT thread and never grown: a
 * push past capacity is counted and dropped rather than reallocating on the audio
 * thread. Events carry the cycle-relative frame and a small port index the backend
 * chose; sort() orders them by (port, frame, insertion), which is the order every
 * MIDI sink requires and keeps same-frame events in the order they were emitted.
 */
class CycleEventBuffer {
public:
    struct Event {
        uint32_t frame;
        uint32_t seq;
        int16_t  port;
        uint8_t  len;
        uint8_t  data[3];
    };

    explicit CycleEventBuffer(size_t capacity)
        : buf(capacity ? new Event[capacity] : nullptr), cap(capacity) {}

    void clear() { count = 0; }
    // False if full; the event is dropped.
    bool push(int port, uint32_t frame, const uint8_t* data, int len);
    void sort();

    const Event* begin() const { return buf.get(); }
    const Event* end()   const { return buf.get() + count; }
    size_t       size()  const { return count; }

    // The sorted run of events for one port (call after sort()).
    template <typename Fn> void forEachOnPort(int port, Fn&& fn) const {
        for (const Event* e = begin(); e != end(); ++e)
            if (e->port == port) fn(*e);
            else if (e->port > port) break;
    }

private:
    std::unique_ptr<Event[]> buf;
    size_t cap   = 0;
    size_t count = 0;
};

class CycleDriver : public Sequencer {
public:
    const RtStats& rtStats() const { return stats; }

protected:
    // eventCapacity: per-cycle events the backend buffers itself. A backend that
    // sends everything through emitUnbuffered() can pass 0.
    CycleDriver(double sampleRate, size_t eventCapacity);

    // RT thread. Renders the window `c` describes into `events` (left sorted) and
    // emitUnbuffered(). Does not clear `events`, so a backend can queue messages of
    // its own for the cycle first — at frame 0 they sort ahead of the engine's.
    // Every cycle renders: the engine never waits on, or skips for, the owner.
    void runCycle(const ClockSample& c);

    int64_t cycleFrame()  const { return curFrame; }
    int64_t cycleFrames() const { return curFrames; }
    double  sampleRate()  const { return sr; }
    void    setDriverSampleRate(double rate) { sr = rate; setSampleRate(rate); }

    // Port name -> index into the backend's per-cycle buffers, or -1 for a port it
    // has none for. RT thread, during runCycle(); must not allocate.
    virtual int resolvePort(const std::string& port) = 0;

    // A message for a port resolvePort() returned -1 for, at cycle-relative frame
    // `offset` (already clamped into the window). RT thread; must not allocate.
    // The default drops it.
    virtual void emitUnbuffered(const std::string& /*port*/, long /*offset*/,
                                const uint8_t* /*data*/, int /*len*/) {}

    CycleEventBuffer events;
    RtStats          stats;

private:
    void emit(const std::string& port, double bar,
              const uint8_t* data, int len) final;

    double  sr          = 48000.0;
    int64_t curFrame    = 0;
    int64_t curFrames   = 0;
};

#endif
