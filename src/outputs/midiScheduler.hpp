// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MIDI_SCHEDULER_HPP
#define MIDI_SCHEDULER_HPP

#include "rtThread.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

/*
 * MidiScheduler — delivers timestamped MIDI to ports that have no sample clock of
 * their own (Native, Debug, and Jack ports while JACK is not the clock), at the
 * moment each message is due.
 *
 * The sequencing engines run on real-time threads and must not call into MIDI
 * drivers: RtMidi's sendMessage() can block (ALSA's output buffer, WinMME), and the
 * Debug port prints. So an engine only push()es — a copy into a lock-free ring plus
 * one atomic — and this class's own sender thread does the sending. That is the
 * split Ardour's WinMME backend uses: the process thread queues timestamped events,
 * a separate high-priority thread sleeps until each is due and hands it to the
 * driver. The sender is promoted to real-time priority for punctuality, but it is
 * not an RT thread in the sense of CLAUDE.md: it may block, and nothing waits on it.
 *
 * There is one single-producer ring per engine (see Producer). Each engine is the
 * only writer of its ring, so the moment a clock switch has both engines emitting
 * needs no extra synchronisation.
 *
 * The sender sleeps until the earliest queued event is due, or until a push()
 * brings an earlier one; with nothing queued it blocks outright. No polling.
 */
class MidiScheduler {
public:
    enum class Producer { Internal = 0, Jack = 1 };
    static constexpr int kProducers = 2;
    static constexpr int kPortNameLen = 64;

    // Called on the sender thread for each message as it falls due.
    using Sink = std::function<void(const char* port, const uint8_t* data, int len)>;

    explicit MidiScheduler(Sink sink);
    ~MidiScheduler();
    MidiScheduler(const MidiScheduler&)            = delete;
    MidiScheduler& operator=(const MidiScheduler&) = delete;

    // RT-safe. Queue a 1-3 byte message for `port`, to be sent at rtNowNs() == dueNs.
    // A full ring drops the message (false) rather than block.
    bool push(Producer from, const char* port, int64_t dueNs, const uint8_t* data, int len);

    // Whether the sender thread got real-time priority (known once constructed).
    bool realtime() const { return realtime_; }

private:
    struct Event {
        int64_t  dueNs;
        uint32_t seq;
        uint8_t  len;
        uint8_t  data[3];
        char     port[kPortNameLen];
    };

    // Fixed-capacity single-producer / single-consumer ring.
    struct Ring {
        static constexpr uint32_t kSize = 8192;   // power of two
        std::unique_ptr<Event[]> buf{new Event[kSize]};
        std::atomic<uint32_t>    head{0};   // next write, producer-owned
        std::atomic<uint32_t>    tail{0};   // next read, consumer-owned
        uint32_t                 seq = 0;   // producer-owned order within equal times
    };

    void run();
    bool ringsEmpty() const;

    Sink sink_;
    Ring rings_[kProducers];

    // Sender-thread state. heap_ is reserved to hold every ring full, so it never
    // reallocates.
    std::vector<Event> heap_;
    uint32_t           drainSeq_ = 0;

    // When the sender will next wake on its own; a push() earlier than this posts
    // the semaphore. INT64_MAX while blocked with nothing queued.
    std::atomic<int64_t> nextDeadline_{INT64_MAX};
    std::atomic<bool>    running_{true};
    RtSemaphore          wake_;
    RtSemaphore          started_;          // posted once the sender has promoted itself
    bool                 realtime_ = false; // written before started_ is posted
    std::thread          thread_;
};

#endif
