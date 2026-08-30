// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

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
//
// Double, not float: windowStart/windowEnd are derived from an absolute song
// position, so late in a long song (or a long loop region) a float's ~1e-7 relative
// step is already worth a sample or more, and the per-cycle window is a *difference*
// of two such values — precisely where that error bites.
template <typename Sink>
inline void forEachFiring(double eventBeat, double patternLen,
                          double windowStart, double windowEnd, Sink&& sink)
{
    if (patternLen <= 0.0) return;
    double cycles    = std::floor((windowStart - eventBeat) / patternLen);
    double firstFire = eventBeat + cycles * patternLen;
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
