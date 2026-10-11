// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "midiScheduler.hpp"
#include <algorithm>
#include <cstring>

namespace {
// Min-heap on (due time, arrival order): std::*_heap build a max-heap, so "less"
// here means "later".
struct Later {
    template <class E> bool operator()(const E& a, const E& b) const {
        if (a.dueNs != b.dueNs) return a.dueNs > b.dueNs;
        return a.seq > b.seq;
    }
};
}

MidiScheduler::MidiScheduler(Sink sink) : sink_(std::move(sink))
{
    heap_.reserve(Ring::kSize * kProducers);
    thread_ = std::thread([this] { run(); });
}

MidiScheduler::~MidiScheduler()
{
    running_.store(false);
    wake_.post();
    if (thread_.joinable()) thread_.join();
}

bool MidiScheduler::push(Producer from, const char* port, int64_t dueNs,
                         const uint8_t* data, int len)
{
    if (len < 1 || len > 3) return false;
    Ring& r = rings_[static_cast<int>(from)];
    const uint32_t head = r.head.load(std::memory_order_relaxed);
    if (head - r.tail.load(std::memory_order_acquire) >= Ring::kSize) return false;

    Event& e = r.buf[head & (Ring::kSize - 1)];
    e.dueNs = dueNs;
    e.seq   = r.seq++;
    e.len   = static_cast<uint8_t>(len);
    std::memcpy(e.data, data, len);
    std::strncpy(e.port, port, kPortNameLen - 1);
    e.port[kPortNameLen - 1] = '\0';

    // seq_cst on both sides (here, and the sender's deadline store then ring check)
    // so that either the sender sees this event before it sleeps, or we see its
    // deadline and wake it.
    r.head.store(head + 1, std::memory_order_seq_cst);
    if (dueNs < nextDeadline_.load(std::memory_order_seq_cst))
        wake_.post();
    return true;
}

bool MidiScheduler::ringsEmpty() const
{
    for (const Ring& r : rings_)
        if (r.head.load(std::memory_order_seq_cst) != r.tail.load(std::memory_order_relaxed))
            return false;
    return true;
}

void MidiScheduler::run()
{
    rtPromoteCurrentThread(1000000);

    while (running_.load()) {
        // Take everything queued into the time-ordered heap. Arrival order across
        // the rings is re-stamped so equal due times keep a stable order.
        for (Ring& r : rings_) {
            uint32_t tail = r.tail.load(std::memory_order_relaxed);
            const uint32_t head = r.head.load(std::memory_order_acquire);
            for (; tail != head; ++tail) {
                Event e = r.buf[tail & (Ring::kSize - 1)];
                e.seq = drainSeq_++;
                heap_.push_back(e);
                std::push_heap(heap_.begin(), heap_.end(), Later{});
            }
            r.tail.store(tail, std::memory_order_release);
        }

        // Send whatever has fallen due.
        const int64_t now = rtNowNs();
        while (!heap_.empty() && heap_.front().dueNs <= now) {
            std::pop_heap(heap_.begin(), heap_.end(), Later{});
            const Event& e = heap_.back();
            sink_(e.port, e.data, e.len);
            heap_.pop_back();
        }

        const int64_t deadline = heap_.empty() ? INT64_MAX : heap_.front().dueNs;
        nextDeadline_.store(deadline, std::memory_order_seq_cst);
        if (!ringsEmpty()) continue;   // a push raced the store above

        if (deadline == INT64_MAX) wake_.wait();
        else                       wake_.waitUntil(deadline);
    }
}
