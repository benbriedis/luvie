// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SLICE_CLIP_HPP
#define SLICE_CLIP_HPP

#include "patternData.hpp"   // PatternType
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// A time slice of a pattern: everything between two beats, top to bottom —
// notes, drum hits and the points of every automation lane. Held by value, like the item
// clipboard, so what gets pasted stays what was copied whatever happens to the
// source in between. Every position is relative to the slice's start.

// A pianoroll note keeps its MIDI note in `row`. A harmony note instead keeps
// its chord degree and the pitch group it sits in, which mean the same thing in
// any chord: the destination re-encodes them for its own chord size, and a
// degree it has no room for becomes a bonus note, as a chord change does.
struct SliceNote {
    int   row        = 0;    // pianoroll only
    int   pitchGroup = 0;    // harmony only
    int   degree     = 0;    // harmony only
    float dBeat      = 0.0f; // may be negative: a note overlapping the start is carried whole
    float length     = 0.0f;
    float velocity   = 0.0f;
};

struct SliceDrum {
    int   note     = 0;
    float dBeat    = 0.0f;
    float velocity = 0.0f;
};

// One automation lane's share of the slice: the points lying in it, and nothing
// else. Carried even when empty, so a paste creates the lane where it is missing.
struct SliceLane {
    std::string type;
    std::vector<std::pair<float, int>> points;   // (dBeat, value)
};

struct SliceClip {
    PatternType            type   = PatternType::HARMONY;
    float                  length = 0.0f;
    std::vector<SliceNote> notes;
    std::vector<SliceDrum> drums;
    std::vector<SliceLane> lanes;
};

// The range of start beats the slice can be placed at in a pattern of
// `patternBeats`: the slice itself must fit, and so must the notes it carries
// whole past either edge. Empty (lo > hi) when there is nowhere it fits.
inline void sliceFitLimits(const SliceClip& clip, float patternBeats, float& lo, float& hi)
{
    lo = 0.0f;
    hi = patternBeats - clip.length;
    for (const SliceNote& n : clip.notes) {
        lo = std::max(lo, -n.dBeat);
        hi = std::min(hi, patternBeats - (n.dBeat + n.length));
    }
}

#endif
