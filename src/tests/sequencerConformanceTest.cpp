// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

// Conformance test for the shared RT engine (Sequencer + CycleDriver), headless and
// deterministic. A project is rendered through a fake clock into a capturing sink,
// once in small fixed buffers as the reference, and then again under the conditions
// real backends meet:
//   - any buffer size, fixed or changing every cycle;
//   - a host clock that wanders (through the plugin's HostClockFollower);
//   - snapshot rebuilds racing the RT thread, as every UI edit and every plugin
//     state message causes;
//   - tempo and meter markers beyond the song-loop end;
//   - a relocate, a stop, and Loop <-> Song hand-offs.
// Re-blocking alone must reproduce the reference event for event (a frame of
// rounding allowed); everything must pass MidiInvariantChecker with no note
// clipped by a reset that was not asked for, and nothing left sounding after a stop.
//
// Usage: sequencer_conformance_test <fixture.json>

#include "cycleDriver.hpp"
#include "hostClockFollower.hpp"
#include "loopManager.hpp"
#include "midiInvariantChecker.hpp"
#include "observableSong.hpp"
#include "timelineIO.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) failures++;
}

constexpr double kRate = 48000.0;

struct Ev {
    int     port;
    int64_t frame;
    uint8_t len, d[3];
};

bool isOff(const Ev& e) { return (e.d[0] & 0xF0) == 0x80 || ((e.d[0] & 0xF0) == 0x90 && e.d[2] == 0); }

// The engine under a fake clock. Every buffered event is captured at its absolute
// frame, the way a backend would write it out.
class TestDriver : public CycleDriver {
public:
    TestDriver() : CycleDriver(kRate, 8192) {}

    void setPorts(const std::vector<std::string>& names) { ports = names; }

    // Setup runs with the setters applying directly; attach once cycles start, so
    // what follows reaches the engine as it would from a real owner thread.
    void attach() { attachRt(true); }

    void cycle(const ClockSample& c)
    {
        if (!c.rolling && stopFrame == INT64_MAX) stopFrame = c.frame;
        events.clear();
        runCycle(c);
        for (const auto& e : events)
            out.push_back({e.port, c.frame + e.frame, e.len,
                           {e.data[0], e.data[1], e.data[2]}});
    }

    std::vector<Ev> out;
    int64_t         stopFrame = INT64_MAX;   // the first stopped cycle

protected:
    int resolvePort(const std::string& name) override
    {
        for (size_t i = 0; i < ports.size(); i++)
            if (ports[i] == name) return static_cast<int>(i);
        return 0;
    }

private:
    std::vector<std::string> ports;
};

// A project, loaded and wired the way the plugin's applyState() wires it.
struct Rig {
    std::unique_ptr<ObservableSong> song;
    LoopManager                     loopMgr;
    TestDriver                      drv;

    explicit Rig(const AppState& st)
        : song(std::make_unique<ObservableSong>(120.0f, 4, 4))
    {
        std::vector<std::string> names;
        for (const auto& o : st.jackOutputs) names.push_back(o.portName);
        drv.setPorts(names);
        std::vector<Sequencer::InstrumentRouting> routings;
        for (const auto& ci : st.jackInstruments)
            routings.push_back({ci.id, ci.portName, ci.midiChannel,
                                ci.programNumber, ci.bankMsb, ci.bankLsb});
        drv.setInstruments(routings);
        drv.setTimeline(song.get());
        drv.setLoopManager(&loopMgr);
        song->loadTimeline(st.timeline);
        drv.attach();
    }
};

// Buffer sizes for a run: either fixed, or drawn fresh every cycle.
struct Blocking {
    int  fixed = 0;              // > 0: always this
    int  lo = 16, hi = 4096;     // otherwise uniform in [lo, hi]
    std::mt19937 rng{12345};
    int next() { return fixed > 0 ? fixed : std::uniform_int_distribution<int>(lo, hi)(rng); }
};

// Roll the transport from frame 0 for `secs`, then stop for a cycle.
void playFor(TestDriver& d, double secs, Blocking b)
{
    const int64_t endFrame = static_cast<int64_t>(secs * kRate);
    int64_t f = 0;
    while (f < endFrame) {
        // The last buffer is cut short, so every blocking stops on the same frame.
        const int n = static_cast<int>(std::min<int64_t>(b.next(), endFrame - f));
        d.cycle({f, n, true, false});
        f += n;
    }
    d.cycle({f, 256, false, false});
}

struct Verdict {
    MidiInvariantChecker::Counts bad;
    MidiInvariantChecker::Stats  stats;
    int held = 0;
};

// The stop's own reset is meant to cut what is sounding, so cutByReset counts only
// what was cut before the transport stopped.
Verdict audit(const TestDriver& d)
{
    auto chk = std::make_unique<MidiInvariantChecker>();
    uint32_t cutBeforeStop = UINT32_MAX;
    for (const auto& e : d.out) {
        if (e.frame >= d.stopFrame && cutBeforeStop == UINT32_MAX)
            cutBeforeStop = chk->noteStats().cutByReset;
        chk->onEvent(e.port, e.frame, e.d, e.len);
    }
    Verdict v{chk->violations(), chk->noteStats(), chk->heldCount()};
    if (cutBeforeStop != UINT32_MAX) v.stats.cutByReset = cutBeforeStop;
    return v;
}

bool clean(const Verdict& v) { return v.bad.total() == 0 && v.held == 0; }

void report(const char* label, const Verdict& v)
{
    printf("    %s: noteOns=%u cutByReset=%u doubleOn=%u orphanOff=%u zeroLength=%u "
           "backwards=%u held=%d\n", label, v.stats.noteOns, v.stats.cutByReset,
           v.bad.doubleOn, v.bad.orphanOff, v.bad.zeroLength, v.bad.backwards, v.held);
}

// Events must match one for one: same port and bytes, frames within `tol`. The order
// of different messages sharing a frame is not significant, so each event is matched
// against any unused identical one within the tolerance.
bool sameStream(const std::vector<Ev>& ref, const std::vector<Ev>& got, int64_t tol,
                int64_t& worst)
{
    worst = 0;
    bool ok = true;
    for (int port = 0; port < MidiInvariantChecker::kPorts; port++) {
        std::vector<Ev> a, b;
        for (const auto& e : ref) if (e.port == port) a.push_back(e);
        for (const auto& e : got) if (e.port == port) b.push_back(e);
        std::vector<bool> used(b.size(), false);
        std::vector<Ev>   missing;
        size_t lo = 0;
        for (const Ev& x : a) {
            while (lo < b.size() && b[lo].frame < x.frame - tol) lo++;
            bool found = false;
            for (size_t j = lo; j < b.size() && b[j].frame <= x.frame + tol; j++) {
                if (used[j] || b[j].len != x.len || std::memcmp(b[j].d, x.d, x.len) != 0)
                    continue;
                used[j] = true;
                found   = true;
                worst   = std::max<int64_t>(worst, std::llabs(b[j].frame - x.frame));
                break;
            }
            if (!found) missing.push_back(x);
        }
        int shown = 0;
        for (const Ev& x : missing) {
            if (shown++ < 6)
                printf("    port %d: missing %02X %02X %02X @%ld (bar %.4f at 80 BPM)\n",
                       port, x.d[0], x.d[1], x.d[2], (long)x.frame, x.frame / kRate / 3.0);
            ok = false;
        }
        for (size_t j = 0; j < b.size(); j++)
            if (!used[j]) {
                if (shown++ < 12)
                    printf("    port %d: extra   %02X %02X %02X @%ld (bar %.4f at 80 BPM)\n",
                           port, b[j].d[0], b[j].d[1], b[j].d[2], (long)b[j].frame,
                           b[j].frame / kRate / 3.0);
                ok = false;
            }
    }
    return ok;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture.json>\n", argv[0]); return 2; }
    std::string json;
    {
        FILE* in = fopen(argv[1], "rb");
        if (!in) { perror(argv[1]); return 2; }
        char b[4096]; size_t n;
        while ((n = fread(b, 1, sizeof b, in)) > 0) json.append(b, n);
        fclose(in);
    }
    AppState st;
    if (!appStateFromJsonString(json, st)) { fprintf(stderr, "bad fixture\n"); return 2; }

    // The fixture's song-loop region, as the plugin's restore applies it.
    const float loopStart = static_cast<float>(st.songLoopStartCol);
    const float loopEnd   = static_cast<float>(st.songLoopEndCol) + 1.0f;
    // ~7 passes of the loop at 80 BPM. Ending off the beat grid keeps the stop from
    // landing on a note, where the 1-frame rounding the comparison allows would
    // decide whether that note is in the run at all.
    const double secs     = 90.0123;

    printf("reference: song loop [%.0f,%.0f), 32-frame buffers, %.0f s\n",
           loopStart, loopEnd, secs);
    Rig ref(st);
    ref.drv.setSongLoop(true, loopStart, loopEnd);
    { Blocking b; b.fixed = 32; playFor(ref.drv, secs, b); }
    const Verdict rv = audit(ref.drv);
    report("reference", rv);
    check(rv.stats.noteOns > 0, "reference plays notes");
    check(clean(rv) && rv.stats.cutByReset == 0, "reference passes the invariants");

    printf("re-blocking: the same song in other buffer sizes\n");
    for (int fixed : {64, 100, 256, 1000, 1024, 4096, 0}) {
        Rig r(st);
        r.drv.setSongLoop(true, loopStart, loopEnd);
        Blocking b; b.fixed = fixed;
        playFor(r.drv, secs, b);
        int64_t worst = 0;
        const bool same = sameStream(ref.drv.out, r.drv.out, 1, worst);
        const Verdict v = audit(r.drv);
        char label[96];
        snprintf(label, sizeof label, "%s buffers: identical to reference (worst %ld frame)",
                 fixed ? std::to_string(fixed).c_str() : "random", (long)worst);
        check(same && clean(v) && v.stats.cutByReset == 0, label);
    }

    printf("host wander: Positions up to 100 ms off, via HostClockFollower\n");
    for (int64_t wander : {-4096, -300, 700, 4700}) {
        Rig r(st);
        r.drv.setSongLoop(true, loopStart, loopEnd);
        HostClockFollower clk(kRate);
        int64_t d = 0, truth = 0;
        clk.onPosition(true, 0, true, 1.0f, 256, d);
        const int64_t endFrame = static_cast<int64_t>(secs * kRate);
        for (int c = 0; truth < endFrame; c++) {
            // Every 37th cycle the host misreports, then corrects itself.
            if (c % 37 == 5) clk.onPosition(true, truth + wander, true, 1.0f, 256, d);
            if (c % 37 == 6) clk.onPosition(true, truth, true, 1.0f, 256, d);
            r.drv.cycle(clk.sample(256));
            clk.endCycle(256);
            truth += 256;
        }
        clk.onPosition(false, 0, true, 0.0f, 256, d);
        r.drv.cycle(clk.sample(256));
        Blocking b; b.fixed = 256;
        Rig r2(st);
        r2.drv.setSongLoop(true, loopStart, loopEnd);
        playFor(r2.drv, static_cast<double>(truth) / kRate, b);
        int64_t worst = 0;
        char label[96];
        snprintf(label, sizeof label, "wander of %ld frames is ridden through, nothing clipped",
                 (long)wander);
        check(sameStream(r2.drv.out, r.drv.out, 0, worst)
              && r.drv.rtStats().relocates.load() == 0, label);
    }

    printf("rebuild storm: snapshot rebuilds racing the RT thread\n");
    {
        Rig r(st);
        r.drv.setSongLoop(true, loopStart, loopEnd);
        // Rebuilds come from the owner thread while cycles run here: what the plugin
        // sees on every UI edit and state message.
        std::atomic<bool> done{false};
        std::thread owner([&] {
            while (!done.load()) {
                r.drv.onTimelineChanged();
                std::this_thread::yield();
            }
        });
        Blocking b; b.fixed = 256;
        playFor(r.drv, secs, b);
        done.store(true);
        owner.join();
        Rig r2(st);
        r2.drv.setSongLoop(true, loopStart, loopEnd);
        playFor(r2.drv, secs, b);
        int64_t worst = 0;
        check(sameStream(r2.drv.out, r.drv.out, 0, worst), "nothing dropped or delayed");
        check(clean(audit(r.drv)), "invariants hold");
    }

    printf("tempo and meter change beyond the loop end\n");
    {
        AppState st2 = st;
        st2.timeline.bpms.push_back({7, 140.0f});
        st2.timeline.timeSigs.push_back({8, 7, 8});
        Rig r(st2);
        r.drv.setSongLoop(true, loopStart, loopEnd);
        Blocking b; b.fixed = 32;
        playFor(r.drv, secs, b);
        Rig r2(st2);
        r2.drv.setSongLoop(true, loopStart, loopEnd);
        b.fixed = 0;
        playFor(r2.drv, secs, b);
        int64_t worst = 0;
        check(sameStream(r.drv.out, r2.drv.out, 1, worst) && clean(audit(r2.drv)),
              "the loop plays the same in any buffer size");
    }

    printf("relocate and stop\n");
    {
        Rig r(st);
        r.drv.setSongLoop(true, loopStart, loopEnd);
        int64_t f = 0;
        for (int c = 0; c < 4000; c++) {
            const bool jump = c == 1500;
            if (jump) f = static_cast<int64_t>(4.5 * 3.0 * kRate);   // on to bar 4.5
            r.drv.cycle({f, 256, true, jump});
            f += 256;
        }
        r.drv.cycle({f, 256, false, false});
        const Verdict v = audit(r.drv);
        report("relocate", v);
        check(clean(v), "invariants hold and nothing sounds after the stop");
        check(r.drv.rtStats().relocates.load() == 1, "the relocate is acted on once");
    }

    printf("Loop <-> Song hand-offs\n");
    {
        Rig r(st);
        r.drv.setSongLoop(true, loopStart, loopEnd);
        int patId = st.timeline.patterns.empty() ? 0 : st.timeline.patterns.front().id;
        int64_t f = 0;
        for (int c = 0; c < 6000; c++) {
            if (c == 100) {
                r.song->holdTempo(1.0f);
                r.loopMgr.mirror({{patId, 0.0f}}, {}, {});
                r.drv.beginLoopMode(2.0f);
            }
            if (c == 2500) {
                r.song->releaseTempoHold();
                r.drv.endLoopMode(3.0f);
            }
            r.drv.cycle({f, 256, true, false});
            f += 256;
        }
        r.drv.cycle({f, 256, false, false});
        const Verdict v = audit(r.drv);
        report("hand-offs", v);
        check(clean(v) && v.stats.cutByReset == 0, "hand-offs clip nothing and hold no note");
    }

    printf(failures ? "FAILED (%d)\n" : "passed\n", failures);
    return failures ? 1 : 0;
}
