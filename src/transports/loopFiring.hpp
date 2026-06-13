#ifndef LOOP_FIRING_HPP
#define LOOP_FIRING_HPP

#include <cmath>

// Shared sequencing helpers used by every clock path: the JACK real-time engine
// (jackTransport), the internal chrono clock and the LV2 host-driven clock (the
// latter two both sequence through Playhead's soft output). Keeping the wrap
// math in one place stops the JACK and soft paths from drifting apart.

// A pattern's notes/params repeat every `patternLen` beats. Given a firing
// window in pattern-relative beats [windowStart, windowEnd), invoke
// sink(fireBeat) once for every occurrence of an event anchored at `eventBeat`
// (modulo patternLen) that lands in the window. fireBeat is reported in the same
// pattern-relative beat space as windowStart / eventBeat.
//
// Header-only and allocation-free, so it is safe to call on the JACK RT thread.
template <typename Sink>
inline void forEachFiring(float eventBeat, float patternLen,
                          float windowStart, float windowEnd, Sink&& sink)
{
    if (patternLen <= 0.0f) return;
    float cycles    = std::floor((windowStart - eventBeat) / patternLen);
    float firstFire = eventBeat + cycles * patternLen;
    if (firstFire < windowStart) firstFire += patternLen;
    while (firstFire < windowEnd) {
        sink(firstFire);
        firstFire += patternLen;
    }
}

// Note length given to drum hits, which carry no explicit length (beats).
// Shared so the JACK and soft paths use an identical value.
inline constexpr float drumNoteLen = 0.1f;

#endif
