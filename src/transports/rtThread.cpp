// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "rtThread.hpp"

// Real-time priority for both of our threads. Below JACK's own (its server and
// client threads sit around 70-90 by default) so we never pre-empt the audio graph,
// above everything ordinary.
static constexpr int kLinuxRtPriority = 60;

#if defined(_WIN32)
// ═══ Windows ═══════════════════════════════════════════════════════════════════

#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static int64_t qpcFreq()
{
    static const int64_t f = [] {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        return (int64_t)li.QuadPart;
    }();
    return f;
}

int64_t rtNowNs()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    const int64_t f = qpcFreq();
    // Split to avoid overflowing 64 bits on long uptimes.
    return (li.QuadPart / f) * 1000000000LL + (li.QuadPart % f) * 1000000000LL / f;
}

// One waitable timer per thread. The high-resolution flavour (Windows 10 1803+)
// sleeps to well under a millisecond; without it, raise the system timer resolution
// to 1 ms once, which is what makes an ordinary waitable timer usable at all.
static HANDLE threadTimer()
{
    thread_local HANDLE t = [] {
        HANDLE h = CreateWaitableTimerExW(nullptr, nullptr,
                                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
        if (!h) {
            static const bool raised = [] {
                if (HMODULE m = LoadLibraryA("winmm.dll"))
                    if (auto f = (UINT(WINAPI*)(UINT))GetProcAddress(m, "timeBeginPeriod"))
                        f(1);
                return true;
            }();
            (void)raised;
            h = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }
        return h;
    }();
    return t;
}

// Arm the thread's timer for an absolute deadline (expressed as a relative due time,
// since the timer's own absolute times are wall-clock).
static void armTimer(HANDLE timer, int64_t deadlineNs)
{
    int64_t rel = deadlineNs - rtNowNs();
    if (rel < 0) rel = 0;
    LARGE_INTEGER due;
    due.QuadPart = -(rel / 100);   // negative = relative, in 100 ns units
    SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
}

void rtSleepUntil(int64_t deadlineNs)
{
    HANDLE timer = threadTimer();
    while (rtNowNs() < deadlineNs) {
        if (!timer) { Sleep(1); continue; }
        armTimer(timer, deadlineNs);
        WaitForSingleObject(timer, INFINITE);
    }
}

bool rtPromoteCurrentThread(int64_t /*periodNs*/)
{
    // MMCSS "Pro Audio" is the class Windows reserves for audio threads. avrt.dll is
    // loaded on demand rather than linked, like the JACK shim, so nothing extra has
    // to ship or be present.
    using SetChars = HANDLE (WINAPI*)(LPCWSTR, LPDWORD);
    using SetPrio  = BOOL   (WINAPI*)(HANDLE, int);
    static HMODULE avrt = LoadLibraryA("avrt.dll");
    if (avrt) {
        auto setChars = (SetChars)GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW");
        auto setPrio  = (SetPrio) GetProcAddress(avrt, "AvSetMmThreadPriority");
        DWORD task = 0;
        if (setChars) {
            if (HANDLE h = setChars(L"Pro Audio", &task)) {
                if (setPrio) setPrio(h, 1 /* AVRT_PRIORITY_HIGH */);
                return true;
            }
        }
    }
    return SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) != 0;
}

RtSemaphore::RtSemaphore()  { impl_ = CreateSemaphoreW(nullptr, 0, 0x7FFFFFFF, nullptr); }
RtSemaphore::~RtSemaphore() { if (impl_) CloseHandle((HANDLE)impl_); }
void RtSemaphore::post()    { ReleaseSemaphore((HANDLE)impl_, 1, nullptr); }
void RtSemaphore::wait()    { WaitForSingleObject((HANDLE)impl_, INFINITE); }

bool RtSemaphore::waitUntil(int64_t deadlineNs)
{
    // WaitForSingleObject's own timeout is in whole milliseconds, too coarse for MIDI,
    // so wait on the semaphore and the thread's precise timer together.
    HANDLE timer = threadTimer();
    if (!timer) {
        int64_t ms = (deadlineNs - rtNowNs() + 999999) / 1000000;
        return WaitForSingleObject((HANDLE)impl_, ms > 0 ? (DWORD)ms : 0) == WAIT_OBJECT_0;
    }
    for (;;) {
        if (WaitForSingleObject((HANDLE)impl_, 0) == WAIT_OBJECT_0) return true;
        if (rtNowNs() >= deadlineNs) return false;
        armTimer(timer, deadlineNs);
        HANDLE hs[2] = {(HANDLE)impl_, timer};
        if (WaitForMultipleObjects(2, hs, FALSE, INFINITE) == WAIT_OBJECT_0) return true;
    }
}

#elif defined(__APPLE__)
// ═══ macOS ═════════════════════════════════════════════════════════════════════

#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>

static const mach_timebase_info_data_t& timebase()
{
    static const mach_timebase_info_data_t tb = [] {
        mach_timebase_info_data_t t;
        mach_timebase_info(&t);
        return t;
    }();
    return tb;
}

static uint64_t nsToAbs(int64_t ns)
{
    const auto& tb = timebase();
    return (uint64_t)((__int128)ns * tb.denom / tb.numer);
}

int64_t rtNowNs()
{
    const auto& tb = timebase();
    return (int64_t)((__int128)mach_absolute_time() * tb.numer / tb.denom);
}

void rtSleepUntil(int64_t deadlineNs)
{
    if (deadlineNs <= rtNowNs()) return;
    mach_wait_until(nsToAbs(deadlineNs));
}

bool rtPromoteCurrentThread(int64_t periodNs)
{
    // The policy CoreAudio's own I/O threads run under: "every period, I need this
    // much CPU, finished within the period".
    thread_time_constraint_policy_data_t pol;
    pol.period      = (uint32_t)nsToAbs(periodNs);
    pol.computation = (uint32_t)nsToAbs(periodNs / 4);
    pol.constraint  = (uint32_t)nsToAbs(periodNs);
    pol.preemptible = 1;
    return thread_policy_set(pthread_mach_thread_np(pthread_self()),
                             THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&pol,
                             THREAD_TIME_CONSTRAINT_POLICY_COUNT) == KERN_SUCCESS;
}

RtSemaphore::RtSemaphore()  { impl_ = (void*)dispatch_semaphore_create(0); }
RtSemaphore::~RtSemaphore() { if (impl_) dispatch_release((dispatch_semaphore_t)impl_); }
void RtSemaphore::post()    { dispatch_semaphore_signal((dispatch_semaphore_t)impl_); }
void RtSemaphore::wait()    { dispatch_semaphore_wait((dispatch_semaphore_t)impl_, DISPATCH_TIME_FOREVER); }

bool RtSemaphore::waitUntil(int64_t deadlineNs)
{
    int64_t rel = deadlineNs - rtNowNs();
    if (rel < 0) rel = 0;
    return dispatch_semaphore_wait((dispatch_semaphore_t)impl_,
                                   dispatch_time(DISPATCH_TIME_NOW, rel)) == 0;
}

#else
// ═══ Linux / other POSIX ═══════════════════════════════════════════════════════

#include <cerrno>
#include <ctime>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>

static timespec toTimespec(int64_t ns)
{
    timespec ts;
    ts.tv_sec  = (time_t)(ns / 1000000000LL);
    ts.tv_nsec = (long)(ns % 1000000000LL);
    return ts;
}

int64_t rtNowNs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void rtSleepUntil(int64_t deadlineNs)
{
    const timespec ts = toTimespec(deadlineNs);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr) == EINTR) {}
}

bool rtPromoteCurrentThread(int64_t /*periodNs*/)
{
    sched_param p{};
    const int maxPrio = sched_get_priority_max(SCHED_FIFO);
    p.sched_priority = kLinuxRtPriority < maxPrio ? kLinuxRtPriority : maxPrio;
    return pthread_setschedparam(pthread_self(), SCHED_FIFO, &p) == 0;
}

RtSemaphore::RtSemaphore()
{
    auto* s = new sem_t;
    sem_init(s, 0, 0);
    impl_ = s;
}

RtSemaphore::~RtSemaphore()
{
    auto* s = static_cast<sem_t*>(impl_);
    sem_destroy(s);
    delete s;
}

void RtSemaphore::post() { sem_post(static_cast<sem_t*>(impl_)); }

void RtSemaphore::wait()
{
    while (sem_wait(static_cast<sem_t*>(impl_)) == EINTR) {}
}

bool RtSemaphore::waitUntil(int64_t deadlineNs)
{
    const timespec ts = toTimespec(deadlineNs);
    for (;;) {
        if (sem_clockwait(static_cast<sem_t*>(impl_), CLOCK_MONOTONIC, &ts) == 0) return true;
        if (errno != EINTR) return false;   // ETIMEDOUT
    }
}

#endif
