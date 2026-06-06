#ifndef CHORDS_HPP
#define CHORDS_HPP

#include <string>

struct ChordDef {
    const char* name;
    int         size;
    int         intervals[5];
};

inline constexpr ChordDef chordDefs[] = {
    {"Major",        3, {0, 4, 7,  0,  0}},
    {"Minor",        3, {0, 3, 7,  0,  0}},
    {"Major 7",      4, {0, 4, 7, 11,  0}},
    {"Minor 7",      4, {0, 3, 7, 10,  0}},
    {"Major 9",      5, {0, 4, 7, 11, 14}},
    {"Minor 9",      5, {0, 3, 7, 10, 14}},
    {"Major Pent.",  5, {0, 2, 4,  7,  9}},
    {"Minor Pent.",  5, {0, 3, 5,  7, 10}},
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
