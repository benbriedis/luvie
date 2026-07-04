#ifndef CHORDS_HPP
#define CHORDS_HPP

#include <string>

struct ChordDef {
    const char* name;
    int         size;
    int         intervals[7];
    bool        isScale;   // true = shown under "Scale"; false = under "Chord"
};

inline constexpr ChordDef chordDefs[] = {
    {"Major",             3, {0, 4, 7,  0,  0,  0,  0}, false},
    {"Minor",             3, {0, 3, 7,  0,  0,  0,  0}, false},
    {"Major 7",           4, {0, 4, 7, 11,  0,  0,  0}, false},
    {"Minor 7",           4, {0, 3, 7, 10,  0,  0,  0}, false},
    {"Major 9",           5, {0, 4, 7, 11, 14,  0,  0}, false},
    {"Minor 9",           5, {0, 3, 7, 10, 14,  0,  0}, false},
    {"Major Pent.",       5, {0, 2, 4,  7,  9,  0,  0}, true},
    {"Minor Pent.",       5, {0, 3, 5,  7, 10,  0,  0}, true},
    {"Major",             7, {0, 2, 4,  5,  7,  9, 11}, true},
    {"Minor (asc)",       7, {0, 2, 3,  5,  7,  9, 11}, true},
    {"Minor (desc)",      7, {0, 2, 3,  5,  7,  8, 10}, true},
    {"Minor (harmonic)",  7, {0, 2, 3,  5,  7,  8, 11}, true},
    {"Mixolydian",        7, {0, 2, 4,  5,  7,  9, 10}, true},
    {"Lydian",            7, {0, 2, 4,  6,  7,  9, 11}, true},
    {"Aeolian",           7, {0, 2, 3,  5,  7,  8, 10}, true},
    {"Dorian",            7, {0, 2, 3,  5,  7,  9, 10}, true},
    {"Phrygian",          7, {0, 1, 3,  5,  7,  8, 10}, true},
    {"Locrian",           7, {0, 1, 3,  5,  6,  8, 10}, true},
    {"Wholetone",         6, {0, 2, 4,  6,  8, 10,  0}, true},
};

inline constexpr int numChordDefs = sizeof(chordDefs) / sizeof(chordDefs[0]);

// Map a pattern note row → MIDI pitch for the given root/chord. Shared by the JACK
// RT engine (jackTransport) and the soft (Playhead-driven) output path so both agree.
inline int rowToMidi(int row, int rootPitch, int chordType)
{
    const ChordDef& def = chordDefs[chordType];
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
