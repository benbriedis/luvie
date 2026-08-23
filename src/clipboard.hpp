// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef CLIPBOARD_HPP
#define CLIPBOARD_HPP

#include <algorithm>
#include <utility>
#include <vector>

// Which kind of grid a copy came from. A paste into a different kind is refused:
// a song block, a chord degree, a MIDI pitch and a drum hit all count rows in
// their own units, so the numbers below only mean anything back where they came
// from.
enum class ClipKind { None, SongInstances, HarmonyNotes, PianorollNotes, DrumNotes };

// One copied item, held relative to the top-left corner of what was copied:
// `dRow` counts screen rows downwards and `dBeat` beats to the right. Relative,
// so a paste lands under the cursor rather than back where the original sits;
// and in screen coordinates rather than model ones, so the same two numbers
// serve every grid whatever its rows stand for.
struct ClipItem {
    int   dRow        = 0;
    float dBeat       = 0.0f;
    float length      = 0.0f;
    float velocity    = 0.0f;
    float startOffset = 0.0f;   // song instances only
};

// The one clipboard every editing grid shares, so a copy made in one pattern can
// be pasted into another. It holds values rather than ids because the items it
// was filled from may be edited or deleted before the paste, and what gets
// pasted should stay what was copied.
class Clipboard {
public:
    ClipKind kind = ClipKind::None;
    std::vector<ClipItem> items;

    bool holds(ClipKind k) const { return k != ClipKind::None && kind == k && !items.empty(); }

    // Replace the contents, rebasing on the top-left corner of what was copied so
    // that corner is what lands under the cursor on a paste.
    void set(ClipKind k, std::vector<ClipItem> in)
    {
        if (in.empty()) return;
        int   minRow  = in.front().dRow;
        float minBeat = in.front().dBeat;
        for (const auto& i : in) {
            minRow  = std::min(minRow,  i.dRow);
            minBeat = std::min(minBeat, i.dBeat);
        }
        for (auto& i : in) { i.dRow -= minRow; i.dBeat -= minBeat; }
        kind  = k;
        items = std::move(in);
    }

    void clear() { kind = ClipKind::None; items.clear(); }
};

// Shared by every grid in the process. A function-local static rather than a
// namespace-scope object so it is constructed on first use, whichever
// translation unit gets there first.
inline Clipboard& clipboard()
{
    static Clipboard instance;
    return instance;
}

#endif
