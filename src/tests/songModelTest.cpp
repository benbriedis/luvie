// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

// Headless checks of ObservableSong edits that the Song Editor drives. No window,
// no devices; runs instantly.
//
//   - Cloning a pattern and dragging the clone's row under another instrument keeps
//     the selection on the clone, so the pattern editor edits the clone and not the
//     source track's first pattern.

#include "observableSong.hpp"
#include <cstdio>

namespace {

int failures = 0;
void check(bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++failures;
}

int rowIndexOfLane(const Timeline& tl, int laneId)
{
    for (int i = 0; i < (int)tl.rowOrder.size(); i++)
        if (tl.rowOrder[i].kind == RowKind::Lane && tl.rowOrder[i].id == laneId) return i;
    return -1;
}

int patternIdOfLane(const Timeline& tl, int laneId)
{
    for (const auto& t : tl.tracks)
        for (const auto& l : t.lanes)
            if (l.id == laneId) return l.patternId;
    return -1;
}

void cloneThenDragToOtherInstrument()
{
    printf("clone a pattern, then drag the clone to another instrument\n");
    ObservableSong song(120.0f, 4, 4);
    int instA = song.addInstrument("A", false);
    int instB = song.addInstrument("B", false);

    int trackA = song.addTrack(instA, 0, -1);
    int srcA   = song.addPianorollLane(trackA);
    int trackB = song.addTrack(instB, 0, -1);
    song.addPianorollLane(trackB);

    int clone = song.cloneLane(trackA, srcA);
    check(clone >= 0, "clone created");
    const Timeline& tl = song.get();
    const int clonePat = patternIdOfLane(tl, clone);
    check(clonePat != patternIdOfLane(tl, srcA), "clone has its own pattern");
    check(tl.selectedLaneId == clone, "clone is selected");

    // Drop the clone's row at the very end — under instrument B's track.
    song.moveRow(rowIndexOfLane(tl, clone), (int)tl.rowOrder.size());

    check(song.trackIdForLaneId(clone) == trackB, "clone now belongs to instrument B's track");
    check(tl.selectedLaneId == clone, "clone is still the selected lane");
    check(tl.selectedTrackIndex == song.trackIndexForId(trackB),
          "selected track follows the clone");
    check(tl.patternIdForSelectedLane() == clonePat,
          "pattern editor resolves the clone's pattern");
    // The old fallback: instrument A's first lane.
    const int firstAPat = tl.tracks[song.trackIndexForId(trackA)].lanes[0].patternId;
    check(tl.patternIdForSelectedLane() != firstAPat,
          "pattern editor does not fall back to instrument A's first pattern");
}

} // namespace

int main()
{
    cloneThenDragToOtherInstrument();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks passed\n");
    return 0;
}
