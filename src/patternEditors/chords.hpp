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
    int         intervals[8];
    bool        isScale;   // true = shown under "Scale"; false = under "Chord"
    const char* submenu;   // menu grouping; nullptr/"" = top level, else submenu name
};

inline constexpr ChordDef chordDefs[] = {
    // --- Chords ---
    // Basic triads. The major triad MUST stay at index 0: chordIndexForHash()
    // falls back to 0 for unknown/empty hashes (the historical default).
    {"if0b8i", "Major",             3, {0, 4, 7,  0,  0,  0,  0}, false, "Basic"},
    {"upisim", "Minor",             3, {0, 3, 7,  0,  0,  0,  0}, false, "Basic"},
    {"mbi6ln", "Augmented",         3, {0, 4, 8,  0,  0,  0,  0}, false, "Basic"},
    {"dimtri", "°",                 3, {0, 3, 6,  0,  0,  0,  0}, false, "Basic"},  // dim triad, classical notation
    {"bafxnj", "sus4",              3, {0, 5, 7,  0,  0,  0,  0}, false, "Basic"},
    {"sus2ch", "sus2",              3, {0, 2, 7,  0,  0,  0,  0}, false, "Basic"},

    // Major family (major triad, no dominant b7)
    {"y42556", "maj7",              4, {0, 4, 7, 11,  0,  0,  0}, false, "Major"},
    {"k7m2p4", "maj6",              4, {0, 4, 7,  9,  0,  0,  0}, false, "Major"},
    {"n6m2rt", "add9",              4, {0, 4, 7, 14,  0,  0,  0}, false, "Major"},
    {"x3n9qw", "6\\/9",               5, {0, 4, 7,  9, 14,  0,  0}, false, "Major"},
    {"k9x4nf", "maj9",              5, {0, 4, 7, 11, 14,  0,  0}, false, "Major"},
    {"m7sh11", "maj7(#11)",         5, {0, 4, 7, 11, 18,  0,  0}, false, "Major"},
    {"m9sh11", "maj9(#11)",         6, {0, 4, 7, 11, 14, 18,  0}, false, "Major"},
    {"majsh5", "maj7(#5)",          4, {0, 4, 8, 11,  0,  0,  0}, false, "Major"},
    {"t8b4ws", "maj7(b5)",          4, {0, 4, 6, 11,  0,  0,  0}, false, "Major"},

    // Minor family (minor third)
    {"snu7lw", "min7",              4, {0, 3, 7, 10,  0,  0,  0}, false, "Minor"},
    {"p6b8pw", "min(maj7)",         4, {0, 3, 7, 11,  0,  0,  0}, false, "Minor"},
    {"v2c6ky", "min6",              4, {0, 3, 7,  9,  0,  0,  0}, false, "Minor"},
    {"minad9", "min(add9)",         4, {0, 3, 7, 14,  0,  0,  0}, false, "Minor"},
    {"b8t5rz", "m6\\/9",              5, {0, 3, 7,  9, 14,  0,  0}, false, "Minor"},
    {"q7d3jx", "min9",              5, {0, 3, 7, 10, 14,  0,  0}, false, "Minor"},
    {"v7c3mq", "min9(maj7)",        5, {0, 3, 7, 11, 14,  0,  0}, false, "Minor"},
    {"h8w4zx", "min11",             6, {0, 3, 7, 10, 14, 17,  0}, false, "Minor"},
    {"min13a", "min13",             7, {0, 3, 7, 10, 14, 17, 21}, false, "Minor"},

    // Dominant family (major third + minor 7th)
    {"dom7ch", "7",                 4, {0, 4, 7, 10,  0,  0,  0}, false, "Dominant"},
    {"z4h9kp", "9",                 5, {0, 4, 7, 10, 14,  0,  0}, false, "Dominant"},
    {"r2k6lp", "11",                6, {0, 4, 7, 10, 14, 17,  0}, false, "Dominant"},
    {"p3n9tb", "13",                7, {0, 4, 7, 10, 14, 17, 21}, false, "Dominant"},
    {"h4l9dm", "7(sus4)",           4, {0, 5, 7, 10,  0,  0,  0}, false, "Dominant"},
    {"p9x2tv", "7(b5)",             4, {0, 4, 6, 10,  0,  0,  0}, false, "Dominant"},
    {"j6w3nb", "7(#5)",             4, {0, 4, 8, 10,  0,  0,  0}, false, "Dominant"},
    {"r5k8cq", "7(b9)",             5, {0, 4, 7, 10, 13,  0,  0}, false, "Dominant"},
    {"m3z7hf", "7(#9)",             5, {0, 4, 7, 10, 15,  0,  0}, false, "Dominant"},
    {"d7sh11", "7(#11)",            5, {0, 4, 7, 10, 18,  0,  0}, false, "Dominant"},
    {"c2v6ln", "aug7(b9)",          5, {0, 4, 8, 10, 13,  0,  0}, false, "Dominant"},
    {"w8c5vb", "aug9",              5, {0, 4, 8, 10, 14,  0,  0}, false, "Dominant"},
    {"l3q7dz", "9(b5)",             5, {0, 4, 6, 10, 14,  0,  0}, false, "Dominant"},
    {"b5t2hw", "9(#11)",            6, {0, 4, 7, 10, 14, 18,  0}, false, "Dominant"},
    {"m6d2vc", "13(b9)",            7, {0, 4, 7, 10, 13, 17, 21}, false, "Dominant"},
    {"x4l7kq", "13(b9b5)",          7, {0, 4, 6, 10, 13, 17, 21}, false, "Dominant"},

    // Diminished family
    {"dimjaz", "dim",               3, {0, 3, 6,  0,  0,  0,  0}, false, "Diminished"},  // dim triad, jazz notation
    {"9pz6vx", "dim7",              4, {0, 3, 6,  9,  0,  0,  0}, false, "Diminished"},
    {"mvae2e", "m7(b5)",            4, {0, 3, 6, 10,  0,  0,  0}, false, "Diminished"},

    // Named chords
    {"dream8", "Dream",             4, {0, 5, 6,  7,  0,  0,  0}, false, "Named"},
    {"elktr7", "Elektra",           5, {0, 1, 4,  7,  9,  0,  0}, false, "Named"},
    {"farbn3", "Farben",            5, {0, 4, 8,  9, 11,  0,  0}, false, "Named"},
    {"hnd1rx", "Hendrix",           5, {0, 4, 7, 10, 15,  0,  0}, false, "Named"},
    {"magic7", "Magic",             8, {0, 1, 5,  6, 10, 12, 15, 17}, false, "Named"},
    {"muchrd", "Mu",                4, {0, 2, 4,  7,  0,  0,  0}, false, "Named"},
    {"myst3q", "Mystic",            6, {0, 2, 4,  6,  9, 10,  0}, false, "Named"},
    {"odenap", "Ode to Napoleon",   6, {0, 1, 4,  5,  8,  9,  0}, false, "Named"},
    {"ptr8ka", "Petrushka",         6, {0, 1, 4,  6,  7, 10,  0}, false, "Named"},
    {"sowht4", "So What",           5, {0, 3, 5,  7, 10,  0,  0}, false, "Named"},
    {"trstn5", "Tristan",           4, {0, 3, 6, 10,  0,  0,  0}, false, "Named"},

    // --- Scales ---
    {"m6skzq", "Major Pent.",       5, {0, 2, 4,  7,  9,  0,  0}, true,  nullptr},
    {"on64vt", "Minor Pent.",       5, {0, 3, 5,  7, 10,  0,  0}, true,  nullptr},
    {"gyz07l", "Major",             7, {0, 2, 4,  5,  7,  9, 11}, true,  nullptr},
    {"3zmfqr", "Minor (asc)",       7, {0, 2, 3,  5,  7,  9, 11}, true,  nullptr},
    {"4psiem", "Minor (desc)",      7, {0, 2, 3,  5,  7,  8, 10}, true,  nullptr},
    {"7hdzsf", "Minor (harmonic)",  7, {0, 2, 3,  5,  7,  8, 11}, true,  nullptr},
    {"modion", "Ionian",            7, {0, 2, 4,  5,  7,  9, 11}, true,  "Modes"},
    {"q2vws4", "Dorian",            7, {0, 2, 3,  5,  7,  9, 10}, true,  "Modes"},
    {"wjrku4", "Phrygian",          7, {0, 1, 3,  5,  7,  8, 10}, true,  "Modes"},
    {"n19hmt", "Lydian",            7, {0, 2, 4,  6,  7,  9, 11}, true,  "Modes"},
    {"cgkl5p", "Mixolydian",        7, {0, 2, 4,  5,  7,  9, 10}, true,  "Modes"},
    {"g8suvb", "Aeolian",           7, {0, 2, 3,  5,  7,  8, 10}, true,  "Modes"},
    {"pulps0", "Locrian",           7, {0, 1, 3,  5,  6,  8, 10}, true,  "Modes"},
    {"ktcanc", "Whole tone",        6, {0, 2, 4,  6,  8, 10,  0}, true,  nullptr},
    {"oct8wh", "Whole-half",        8, {0, 2, 3,  5,  6,  8,  9, 11}, true, "Octatonic"},
    {"oct8hw", "Half-whole",        8, {0, 1, 3,  4,  6,  7,  9, 10}, true, "Octatonic"},
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

// Semitone offset above the root for the n-th tone, stacking successive octave
// groups exactly one real octave (12 semitones) apart. Anchoring every group on a
// +12 root keeps a note's root in the same octave before and after a chord/scale
// change, and lets note conversions work upward from the root (see remapPatternNotes).
// Extended chords (9ths/11ths/13ths) reach above the octave, so their upper tones can
// overlap the next group's lower tones — the row sequence is not strictly ascending
// for those chords, the accepted trade-off for keeping the root octave-stable.
inline int chordToneOffset(const ChordDef& def, int n)
{
    return def.intervals[n % def.size] + (n / def.size) * 12;
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
