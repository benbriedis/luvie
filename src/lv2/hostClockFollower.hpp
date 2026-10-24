// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef HOST_CLOCK_FOLLOWER_HPP
#define HOST_CLOCK_FOLLOWER_HPP

#include "cycleDriver.hpp"   // ClockSample
#include <cstdint>

/*
 * The plugin's clock: the host's time:Position stream turned into one ClockSample
 * per run(). Hosts send a Position only when the transport changes, so between
 * them the frame is our own counter, advanced by each cycle's length while rolling.
 *
 * A host frame that disagrees with that counter is normally a relocate. But Carla
 * on PipeWire-JACK, while rolling, reports frames that wander either side of the
 * true position — for one cycle or for a stretch of several — before settling
 * back. Acting on those means a hard reset (All Notes Off) and a re-rendered
 * window each time, which clips every held note and drops or repeats others. So
 * while rolling, a discrepancy smaller than the wander window is taken as jitter
 * and our own counter kept, however long it lasts.
 *
 * The window is measured in time, not buffers. The wander is the host's period
 * misreporting, which bears no relation to how many frames each run() gets: a
 * host splitting its period into small plugin buffers wanders by many of them. An
 * offset that small is inaudible, whereas acting on it is not, and no real seek
 * is ever that small — a seek comes from a click on a bar ruler. Stopped, there is
 * nothing to clip and the host is followed exactly.
 */
class HostClockFollower {
public:
    // How far, while rolling, a host frame may wander before it counts as a seek.
    static constexpr double kWanderSecs = 0.1;

    enum class Verdict { None, Followed, Jitter, Relocate };

    explicit HostClockFollower(double sampleRate) : sr(sampleRate) {}
    void setSampleRate(double rate) { sr = rate; }

    // A time:Position arrived this cycle. hasFrame/hasSpeed: which fields it carried.
    // `nframes` is this cycle's length. Returns what was made of the frame, and the
    // discrepancy in `delta`, for the debug log.
    Verdict onPosition(bool hasFrame, int64_t hostFrame, bool hasSpeed, float speed,
                       uint32_t nframes, int64_t& delta)
    {
        Verdict v = Verdict::None;
        delta = 0;
        if (hasFrame) {
            delta = hostFrame - frame;
            if (delta == 0) {
                v = Verdict::Followed;
            } else if (rolling && withinWander(delta, nframes)) {
                v = Verdict::Jitter;
            } else {
                v = Verdict::Relocate;
                relocated = true;
                frame = hostFrame;
            }
        }
        if (hasSpeed) rolling = speed != 0.0f;
        return v;
    }

    // This cycle's window. Consumes the relocate flag.
    ClockSample sample(uint32_t nframes)
    {
        ClockSample c;
        c.frame     = frame;
        c.nframes   = nframes;
        c.rolling   = rolling;
        c.relocated = relocated;
        relocated   = false;
        return c;
    }

    // After the cycle has rendered: move on to the next window.
    void endCycle(uint32_t nframes) { if (rolling) frame += nframes; }

    int64_t currentFrame() const { return frame; }
    bool    isRolling()    const { return rolling; }

private:
    bool withinWander(int64_t delta, uint32_t nframes) const
    {
        int64_t tol = static_cast<int64_t>(kWanderSecs * sr);
        if (tol < 2 * static_cast<int64_t>(nframes)) tol = 2 * static_cast<int64_t>(nframes);
        return delta >= -tol && delta <= tol;
    }

    double  sr;
    int64_t frame     = 0;
    bool    rolling   = false;
    bool    relocated = false;
};

#endif
