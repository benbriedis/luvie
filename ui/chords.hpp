#ifndef CHORDS_HPP
#define CHORDS_HPP

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

#endif
