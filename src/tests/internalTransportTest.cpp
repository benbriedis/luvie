// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

// Headless check of the Internal clock: InternalTransport rendering on its RT thread
// and the MidiScheduler delivering what it renders. A four-note drum pattern at
// 240 BPM (one note every 250 ms) is played through a capturing sink, and the test
// checks what arrives and when:
//   - the notes, in order, a beat apart;
//   - a song-loop wrap keeps the beat across the seam;
//   - a seek while playing keeps playing and resets the channel;
//   - a stop releases notes and nothing plays after it.
// Delivery jitter is reported; the median must be under 1 ms.

#include "internalTransport.hpp"
#include "midiScheduler.hpp"
#include "observableSong.hpp"
#include "rtThread.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace {

struct Received { int64_t ns; uint8_t status, d1, d2; };

std::mutex              gMutex;
std::condition_variable gArrived;
std::vector<Received>   gReceived;

constexpr double kBeatNs = 250e6;   // 240 BPM
constexpr int    kKick   = 36;
constexpr int    kSnare  = 38;

int failures = 0;
void check(bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++failures;
}

void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

std::vector<Received> take()
{
    std::lock_guard<std::mutex> lk(gMutex);
    std::vector<Received> out;
    out.swap(gReceived);
    return out;
}

std::vector<Received> noteOns(const std::vector<Received>& v)
{
    std::vector<Received> out;
    for (const auto& r : v)
        if ((r.status & 0xF0) == 0x90 && r.d2 > 0) out.push_back(r);
    return out;
}

// How many note-ons a pause should have let through: one per beat from the first
// note up to the pause, inclusive. A pause landing within a few ms of a beat may or
// may not catch that beat, so the range then admits both.
struct Expect { size_t lo, hi; double beats; };

// Plays, then pauses `beats` beats after the first note-on actually arrived, and
// says how many note-ons to expect. Timing the pause from play() with a plain sleep
// would also count the engine's start-up and the main thread's own wake-up slop,
// which on a loaded CI VM is enough to gain or lose a beat; this way the expectation
// follows when the pause really happened.
Expect playAndPauseAfter(InternalTransport& tr, double beats)
{
    tr.play();
    int64_t firstNs;
    {
        std::unique_lock<std::mutex> lk(gMutex);
        if (!gArrived.wait_for(lk, std::chrono::seconds(2),
                               [] { return !noteOns(gReceived).empty(); })) {
            lk.unlock();
            tr.pause();
            return {1, 1, 0.0};   // nothing played: fail the count
        }
        firstNs = noteOns(gReceived)[0].ns;
    }
    rtSleepUntil(firstNs + static_cast<int64_t>(beats * kBeatNs));
    const int64_t pauseNs = rtNowNs();
    tr.pause();
    sleepMs(50);

    const double sounded = (pauseNs - firstNs) / kBeatNs;
    const size_t n       = static_cast<size_t>(std::floor(sounded)) + 1;
    const double frac    = sounded - std::floor(sounded);
    if (frac < 0.05) return {n - 1, n, sounded};
    if (frac > 0.95) return {n, n + 1, sounded};
    return {n, n, sounded};
}

bool countOk(const std::vector<Received>& ons, const Expect& e)
{
    const bool ok = ons.size() >= e.lo && ons.size() <= e.hi;
    if (!ok)
        printf("  got %zu note-ons, expected %zu..%zu (paused %.3f beats after the first)\n",
               ons.size(), e.lo, e.hi, e.beats);
    return ok;
}

bool hasAllNotesOff(const std::vector<Received>& v)
{
    for (const auto& r : v)
        if ((r.status & 0xF0) == 0xB0 && r.d1 == 123) return true;
    return false;
}

// Deviation of each gap between consecutive note-ons from one beat, in ms.
std::vector<double> gapErrorsMs(const std::vector<Received>& ons)
{
    std::vector<double> errs;
    for (size_t i = 1; i < ons.size(); ++i)
        errs.push_back(std::fabs((ons[i].ns - ons[i - 1].ns) - kBeatNs) / 1e6);
    return errs;
}

Timeline drumSong()
{
    Timeline tl;
    tl.bpms     = {{0, 240.0f}};
    tl.timeSigs = {{0, 4, 4}};
    tl.instruments = {{1, "Drums", true, {}}};

    Pattern p{};
    p.id          = 1;
    p.lengthBeats = 4.0f;
    p.type        = PatternType::DRUM;
    p.instrumentId = 1;
    p.drumNotes   = {{1, kKick, 0.0f}, {2, kSnare, 1.0f}, {3, kKick, 2.0f}, {4, kSnare, 3.0f}};
    tl.patterns   = {p};

    Lane lane{};
    lane.id        = 1;
    lane.patternId = 1;
    lane.patterns  = {{1, 1, 0.0f, 8.0f}};
    Track track{};
    track.id           = 1;
    track.instrumentId = 1;
    track.lanes        = {lane};
    tl.tracks = {track};
    return tl;
}

} // namespace

int main()
{
    MidiScheduler sched([](const char* port, const uint8_t* d, int len) {
        if (std::string(port) != "out") return;
        {
            std::lock_guard<std::mutex> lk(gMutex);
            gReceived.push_back({rtNowNs(), d[0], len > 1 ? d[1] : (uint8_t)0,
                                 len > 2 ? d[2] : (uint8_t)0});
        }
        gArrived.notify_all();
    });

    ObservableSong song(240.0f, 4, 4);
    song.loadTimeline(drumSong());

    InternalTransport tr;
    tr.setTimeline(&song);
    tr.setInstruments({{1, "out", 10}});
    tr.start(&sched);
    std::vector<double> allErrs;

    printf("plain playback\n");
    {
        const Expect want = playAndPauseAfter(tr, 4.5);   // beats 0..4, stopping between notes
        auto got = take();
        auto ons = noteOns(got);
        check(countOk(ons, want), "five note-ons in 4.5 beats");
        bool order = ons.size() >= 4 && ons[0].d1 == kKick && ons[1].d1 == kSnare &&
                     ons[2].d1 == kKick && ons[3].d1 == kSnare;
        check(order, "kick, snare, kick, snare");
        check(!ons.empty() && (ons[0].status & 0x0F) == 9, "on the instrument's channel (10)");
        auto errs = gapErrorsMs(ons);
        allErrs.insert(allErrs.end(), errs.begin(), errs.end());
        check(hasAllNotesOff(got), "stop sends All Notes Off");
    }
    sleepMs(300);
    check(noteOns(take()).empty(), "nothing plays after the stop");

    printf("song loop over bar 0 (four beats)\n");
    tr.rewind();
    tr.setSongLoop(true, 0.0f, 1.0f);
    {
        const Expect want = playAndPauseAfter(tr, 8.5);   // two passes and a bit
        auto ons = noteOns(take());
        check(countOk(ons, want), "nine note-ons across two wraps");
        bool pattern = true;
        for (size_t i = 0; i < ons.size(); ++i)
            pattern &= ons[i].d1 == (i % 2 == 0 ? kKick : kSnare);
        check(pattern, "pattern restarts at each wrap");
        auto errs = gapErrorsMs(ons);
        double worst = errs.empty() ? 0.0 : *std::max_element(errs.begin(), errs.end());
        check(worst < 5.0, "beat spacing holds across the seam");
        allErrs.insert(allErrs.end(), errs.begin(), errs.end());
    }
    tr.setSongLoop(false, 0.0f, 0.0f);

    printf("seek while playing\n");
    tr.rewind();
    tr.play();
    sleepMs(380);            // past beats 0 and 1
    take();
    tr.seek(1.0f);           // bar 1 = beat 4: kick
    sleepMs(130);
    {
        auto got = take();
        auto ons = noteOns(got);
        check(tr.isPlaying(), "still playing after the seek");
        check(hasAllNotesOff(got), "the jump resets the channel");
        check(!ons.empty() && ons[0].d1 == kKick, "playback resumes from the seek target");
    }
    tr.pause();
    sleepMs(50);
    take();

    tr.stop();

    std::sort(allErrs.begin(), allErrs.end());
    const double median = allErrs.empty() ? 0.0 : allErrs[allErrs.size() / 2];
    const double worst  = allErrs.empty() ? 0.0 : allErrs.back();
    printf("timing: %zu gaps, median error %.3f ms, worst %.3f ms\n",
           allErrs.size(), median, worst);
    check(median < 1.0, "median delivery error under 1 ms");

    printf("%s\n", failures ? "FAILED" : "passed");
    return failures ? 1 : 0;
}
