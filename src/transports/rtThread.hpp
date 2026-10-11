// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef RT_THREAD_HPP
#define RT_THREAD_HPP

#include <cstdint>

/*
 * Small platform layer for the threads that drive MIDI without JACK: the Internal
 * transport's engine thread and the MidiScheduler's sender thread. It follows what
 * Ardour's own non-JACK backends do (WinMME output thread, CoreAudio policy):
 *
 *   rtNowNs()        one monotonic clock that everything timestamps against
 *   rtSleepUntil()   an absolute-deadline sleep, so a periodic loop cannot drift
 *   rtPromoteCurrentThread()  ask the OS for real-time scheduling
 *   RtSemaphore      a wakeup whose post() is safe on the RT thread
 *
 * Everything here works without elevated privileges: promotion failing (no rtprio
 * on Linux, say) leaves an ordinary thread, which still runs, just less punctually.
 */

// Monotonic nanoseconds. The same clock rtSleepUntil() and RtSemaphore::waitUntil()
// measure their deadlines against.
int64_t rtNowNs();

// Sleep until rtNowNs() >= deadlineNs. Returns at once for a deadline in the past.
void rtSleepUntil(int64_t deadlineNs);

// Give the calling thread real-time priority, sized for work done every periodNs.
// Linux: SCHED_FIFO. macOS: THREAD_TIME_CONSTRAINT_POLICY. Windows: MMCSS "Pro
// Audio". Returns false (and changes nothing) where the OS refuses.
bool rtPromoteCurrentThread(int64_t periodNs);

// Counting semaphore. post() is async-signal/RT-safe on every platform (it neither
// allocates nor takes a lock a non-RT thread could hold); the waits are for the
// non-RT side, or for an RT thread that has nothing to do.
class RtSemaphore {
public:
    RtSemaphore();
    ~RtSemaphore();
    RtSemaphore(const RtSemaphore&)            = delete;
    RtSemaphore& operator=(const RtSemaphore&) = delete;

    void post();
    void wait();
    // Wait for a post() or until rtNowNs() >= deadlineNs. True if posted.
    bool waitUntil(int64_t deadlineNs);

private:
    void* impl_ = nullptr;
};

#endif
