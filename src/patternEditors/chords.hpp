#ifndef CHORDS_HPP
#define CHORDS_HPP

#include <string>
#include <string_view>

struct ChordDef {
    // Stable, unique 6-char [a-z0-9] identity. The array index and the display
    // name are both unstable (reordering / relabelling), so this hash — not the
    // index — is what gets stored, serialized, and passed around as identity.
    const char* hash;
    const char* name;
    int         size;
    int         intervals[7];
    bool        isScale;   // true = shown under "Scale"; false = under "Chord"
    const char* submenu;   // menu grouping; nullptr/"" = top level, else submenu name
};

inline constexpr ChordDef chordDefs[] = {
    {"if0b8i", "major",             3, {0, 4, 7,  0,  0,  0,  0}, false, nullptr},
    {"upisim", "minor",             3, {0, 3, 7,  0,  0,  0,  0}, false, nullptr},
    {"9pz6vx", "dim7",              4, {0, 3, 6,  9,  0,  0,  0}, false, nullptr},
    {"snu7lw", "min7",              4, {0, 3, 7, 10,  0,  0,  0}, false, nullptr},
    {"y42556", "maj7",              4, {0, 4, 7, 11,  0,  0,  0}, false, nullptr},
    {"p6b8pw", "min(maj7)",         4, {0, 3, 7, 11,  0,  0,  0}, false, nullptr},
    {"mvae2e", "½dim",            4, {0, 3, 6, 10,  0,  0,  0}, false, nullptr},
    {"mbi6ln", "aug",               3, {0, 4, 8,  0,  0,  0,  0}, false, nullptr},
    {"bafxnj", "sus4",              3, {0, 5, 7,  0,  0,  0,  0}, false, nullptr},
    {"m6skzq", "Major Pent.",       5, {0, 2, 4,  7,  9,  0,  0}, true,  nullptr},
    {"on64vt", "Minor Pent.",       5, {0, 3, 5,  7, 10,  0,  0}, true,  nullptr},
    {"gyz07l", "Major",             7, {0, 2, 4,  5,  7,  9, 11}, true,  nullptr},
    {"3zmfqr", "Minor (asc)",       7, {0, 2, 3,  5,  7,  9, 11}, true,  nullptr},
    {"4psiem", "Minor (desc)",      7, {0, 2, 3,  5,  7,  8, 10}, true,  nullptr},
    {"7hdzsf", "Minor (harmonic)",  7, {0, 2, 3,  5,  7,  8, 11}, true,  nullptr},
    {"cgkl5p", "Mixolydian",        7, {0, 2, 4,  5,  7,  9, 10}, true,  nullptr},
    {"n19hmt", "Lydian",            7, {0, 2, 4,  6,  7,  9, 11}, true,  nullptr},
    {"g8suvb", "Aeolian",           7, {0, 2, 3,  5,  7,  8, 10}, true,  nullptr},
    {"q2vws4", "Dorian",            7, {0, 2, 3,  5,  7,  9, 10}, true,  nullptr},
    {"wjrku4", "Phrygian",          7, {0, 1, 3,  5,  7,  8, 10}, true,  nullptr},
    {"pulps0", "Locrian",           7, {0, 1, 3,  5,  6,  8, 10}, true,  nullptr},
    {"ktcanc", "Whole tone",        6, {0, 2, 4,  6,  8, 10,  0}, true,  nullptr},
    {"k7m2p4", "maj6",              4, {0, 4, 7,  9,  0,  0,  0}, false, "Extended"},
    {"x3n9qw", "6(add9)",           5, {0, 4, 7,  9, 14,  0,  0}, false, "Extended"},
    {"b8t5rz", "min6(add9)",        5, {0, 3, 7,  9, 14,  0,  0}, false, "Extended"},
    {"v2c6ky", "min6",              4, {0, 3, 7,  9,  0,  0,  0}, false, "Extended"},
    {"h4l9dm", "7(sus4)",           4, {0, 5, 7, 10,  0,  0,  0}, false, "Extended"},
    {"j6w3nb", "7(aug5)",           4, {0, 4, 8, 10,  0,  0,  0}, false, "Extended"},
    {"p9x2tv", "7(b5)",             4, {0, 4, 6, 10,  0,  0,  0}, false, "Extended"},
    {"r5k8cq", "7(b9)",             5, {0, 4, 7, 10, 13,  0,  0}, false, "Extended"},
    {"m3z7hf", "7(#9)",             5, {0, 4, 7, 10, 15,  0,  0}, false, "Extended"},
    {"t8b4ws", "maj7(b5)",          4, {0, 4, 6, 11,  0,  0,  0}, false, "Extended"},
    {"c2v6ln", "aug7(b9)",          5, {0, 4, 8, 10, 13,  0,  0}, false, "Extended"},
    {"q7d3jx", "min9",              5, {0, 3, 7, 10, 14,  0,  0}, false, "Extended"},
    {"z4h9kp", "9",                 5, {0, 4, 7, 10, 14,  0,  0}, false, "Extended"},
    {"n6m2rt", "add9",              4, {0, 4, 7, 14,  0,  0,  0}, false, "Extended"},
    {"w8c5vb", "aug9",              5, {0, 4, 8, 10, 14,  0,  0}, false, "Extended"},
    {"l3q7dz", "9(b5)",             5, {0, 4, 6, 10, 14,  0,  0}, false, "Extended"},
    {"k9x4nf", "maj9",              5, {0, 4, 7, 11, 14,  0,  0}, false, "Extended"},
    {"b5t2hw", "9(#11)",            6, {0, 4, 7, 10, 14, 18,  0}, false, "Extended"},
    {"v7c3mq", "min9(maj7)",        5, {0, 3, 7, 11, 14,  0,  0}, false, "Extended"},
    {"r2k6lp", "11",                6, {0, 4, 7, 10, 14, 17,  0}, false, "Extended"},
    {"h8w4zx", "min11",             6, {0, 3, 7, 10, 14, 17,  0}, false, "Extended"},
    {"p3n9tb", "13",                7, {0, 4, 7, 10, 14, 17, 21}, false, "Extended"},
    {"m6d2vc", "13(b9)",            7, {0, 4, 7, 10, 13, 17, 21}, false, "Extended"},
    {"x4l7kq", "13(b9b5)",          7, {0, 4, 6, 10, 13, 17, 21}, false, "Extended"},
};

inline constexpr int numChordDefs = sizeof(chordDefs) / sizeof(chordDefs[0]);

// Compile-time guarantee that every hash is unique.
consteval bool chordHashesUnique()
{
    for (int i = 0; i < numChordDefs; ++i)
        for (int j = i + 1; j < numChordDefs; ++j)
            if (std::string_view(chordDefs[i].hash) == chordDefs[j].hash)
                return false;
    return true;
}
static_assert(chordHashesUnique(), "chordDefs hashes must be unique");

// Resolve a stable hash to its current array index. Unknown/empty hashes fall
// back to 0 ("major"), matching the historical default. Linear scan over a tiny
// fixed table — allocation-free, so it is safe to call from the RT thread.
inline int chordIndexForHash(std::string_view hash)
{
    for (int i = 0; i < numChordDefs; ++i)
        if (hash == chordDefs[i].hash) return i;
    return 0;
}

inline const ChordDef& chordDefForHash(std::string_view hash)
{
    return chordDefs[chordIndexForHash(hash)];
}

// Semitone offset above the root for the n-th tone, walking up successive
// octave-groups. Extended chords (9ths/11ths/13ths) span more than one octave,
// so each group must advance by a whole number of octaves large enough to clear
// the chord's top interval — otherwise the wrap lands below the previous tone and
// the sequence dips instead of ascending. For chords/scales that fit in an octave
// this is just +12 per group, matching the historical behaviour.
inline int chordToneOffset(const ChordDef& def, int n)
{
    int span = (def.intervals[def.size - 1] / 12 + 1) * 12;
    return def.intervals[n % def.size] + (n / def.size) * span;
}

// Map a pattern note row → MIDI pitch for the given root/chord. Shared by the JACK
// RT engine (jackTransport) and the soft (Playhead-driven) output path so both agree.
// chordIndex is an already-resolved array index (see chordIndexForHash).
inline int rowToMidi(int row, int rootPitch, int chordIndex)
{
    const ChordDef& def = chordDefs[chordIndex];
    int rootMidi0 = 12 + (rootPitch + 9) % 12;
    int midi = rootMidi0 + chordToneOffset(def, row);
    return midi < 0 ? 0 : (midi > 127 ? 127 : midi);
}

// Map a param-lane type name → MIDI CC number; -1 means pitch bend.
inline int ccForType(const std::string& type)
{
    if (type == "Modulation") return 1;
    if (type == "Volume")     return 7;
    if (type == "Pan")        return 10;
    if (type == "Expression") return 11;
    return -1;  // "Pitch" and unknowns → pitch bend
}

#endif
