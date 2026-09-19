// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PARAM_LANE_TYPES_HPP
#define PARAM_LANE_TYPES_HPP

#include <vector>
#include <variant>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

// Returns the maximum value for a param lane: 16383 for pitch bend, 127 for CC lanes.
inline int laneMaxValue(const std::string& type)
{
    return (type == "Pitch") ? 16383 : 127;
}

// Densify the linear ramp between two automation points into intermediate
// stepped events, invoking sink(beat, value) for each. The endpoints themselves
// are NOT emitted (callers emit the points). CC lanes (range <=127) step by 1, one
// event per integer. Pitch bend uses the full 14-bit range (0-16383); stepping by
// 1 would emit up to 16383 events per segment — flooding the JACK MIDI buffer
// ("memory allocation failed") and the verbose log — so the step is coarsened to
// cap a segment at ~128 events (well below audible quantisation). step==1 keeps CC
// identical. Single source of truth: the RT engine (JackTransport) and the soft
// sequencer (Playhead) both call this so their output can't diverge.
template <typename Sink>
inline void densifyParamRamp(float b0, float b1, int v0, int v1, Sink&& sink)
{
    if (!(b1 > b0) || v1 == v0) return;
    float db   = b1 - b0;
    int   dv   = v1 - v0;
    int   step = std::max(1, std::abs(dv) / 128);
    if (dv > 0)
        for (int N = v0; N < v1; N += step)
            sink(b0 + (N + step * 0.5f - v0) / dv * db, std::min(N + step, v1));
    else
        for (int N = v1; N < v0; N += step)
            sink(b0 + (N + step * 0.5f - v0) / dv * db, N);
}

// Thin a run of automation points (sorted by beat) without changing what plays back
// by more than `tol` value units: Ramer-Douglas-Peucker, with the error measured
// vertically, because playback interpolates the value linearly in beats between
// points (see densifyParamRamp). A straight ramp collapses to its two ends; steps
// and turns survive. Fills keep[0..n) — the first and last are always kept.
inline void thinParamRun(const std::vector<float>& beats, const std::vector<int>& values,
                         double tol, std::vector<char>& keep)
{
    const int n = (int)beats.size();
    keep.assign(n, 0);
    if (n == 0) return;
    keep[0] = keep[n - 1] = 1;
    std::vector<std::pair<int, int>> spans;
    if (n > 2) spans.push_back({0, n - 1});
    while (!spans.empty()) {
        auto [a, b] = spans.back();
        spans.pop_back();
        const double db = beats[b] - beats[a];
        int    worst    = -1;
        double worstErr = tol;
        for (int i = a + 1; i < b; i++) {
            const double t    = db > 0.0 ? (beats[i] - beats[a]) / db : 0.0;
            const double line = values[a] + t * (values[b] - values[a]);
            const double err  = std::abs(values[i] - line);
            if (err > worstErr) { worstErr = err; worst = i; }
        }
        if (worst < 0) continue;
        keep[worst] = 1;
        if (worst - a > 1) spans.push_back({a, worst});
        if (b - worst > 1) spans.push_back({worst, b});
    }
}

// Returns the MIDI default (reset) value for a param lane.
inline int laneDefaultValue(const std::string& type)
{
    if (type == "Pitch")      return 8192;  // no bend
    if (type == "Volume")     return 100;
    if (type == "Pan")        return 64;    // center
    if (type == "Expression") return 127;
    return 0;  // Modulation and unknowns
}

struct ParamPtLocal {
    int   id;
    float beat;
    int   value;
    bool  anchor;
};

struct ParamLaneLocal {
    int                       id;
    std::string               type;
    std::vector<ParamPtLocal> points;  // sorted by beat
};

struct ParamIdle {};
struct ParamPendingCreate { int laneIdx; float beat; int value; };
struct ParamDragState     { int laneIdx, ptIdx; float origBeat; int origValue; bool moved = false; };
struct ParamVirtualDrag   { int laneIdx, predPtIdx, origValue; bool moved = false; };
using ParamState = std::variant<ParamIdle, ParamPendingCreate, ParamDragState, ParamVirtualDrag>;

#endif
