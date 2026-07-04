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
};

inline constexpr ChordDef chordDefs[] = {
    {"if0b8i", "major",             3, {0, 4, 7,  0,  0,  0,  0}, false},
    {"upisim", "minor",             3, {0, 3, 7,  0,  0,  0,  0}, false},
    {"9pz6vx", "dim7",              4, {0, 3, 6,  9,  0,  0,  0}, false},
    {"snu7lw", "min7",              4, {0, 3, 7, 10,  0,  0,  0}, false},
    {"y42556", "maj7",              4, {0, 4, 7, 11,  0,  0,  0}, false},
    {"p6b8pw", "min(maj7)",         4, {0, 3, 7, 11,  0,  0,  0}, false},
    {"mvae2e", "½dim",            4, {0, 3, 6, 10,  0,  0,  0}, false},
    {"mbi6ln", "aug",               3, {0, 4, 8,  0,  0,  0,  0}, false},
    {"bafxnj", "sus4",              3, {0, 5, 7,  0,  0,  0,  0}, false},
    {"m6skzq", "Major Pent.",       5, {0, 2, 4,  7,  9,  0,  0}, true},
    {"on64vt", "Minor Pent.",       5, {0, 3, 5,  7, 10,  0,  0}, true},
    {"gyz07l", "Major",             7, {0, 2, 4,  5,  7,  9, 11}, true},
    {"3zmfqr", "Minor (asc)",       7, {0, 2, 3,  5,  7,  9, 11}, true},
    {"4psiem", "Minor (desc)",      7, {0, 2, 3,  5,  7,  8, 10}, true},
    {"7hdzsf", "Minor (harmonic)",  7, {0, 2, 3,  5,  7,  8, 11}, true},
    {"cgkl5p", "Mixolydian",        7, {0, 2, 4,  5,  7,  9, 10}, true},
    {"n19hmt", "Lydian",            7, {0, 2, 4,  6,  7,  9, 11}, true},
    {"g8suvb", "Aeolian",           7, {0, 2, 3,  5,  7,  8, 10}, true},
    {"q2vws4", "Dorian",            7, {0, 2, 3,  5,  7,  9, 10}, true},
    {"wjrku4", "Phrygian",          7, {0, 1, 3,  5,  7,  8, 10}, true},
    {"pulps0", "Locrian",           7, {0, 1, 3,  5,  6,  8, 10}, true},
    {"ktcanc", "Wholetone",         6, {0, 2, 4,  6,  8, 10,  0}, true},
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

// Map a pattern note row → MIDI pitch for the given root/chord. Shared by the JACK
// RT engine (jackTransport) and the soft (Playhead-driven) output path so both agree.
// chordIndex is an already-resolved array index (see chordIndexForHash).
inline int rowToMidi(int row, int rootPitch, int chordIndex)
{
    const ChordDef& def = chordDefs[chordIndex];
    int rootMidi0 = 12 + (rootPitch + 9) % 12;
    int midi = rootMidi0 + def.intervals[row % def.size] + (row / def.size) * 12;
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
