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
    int         intervals[12];
    bool        isScale;   // true = shown under "Scale"; false = under "Chord"
    // Menu grouping; nullptr/"" = top level. A '/' nests, so "World/Japan" puts the
    // entry two levels deep (FLTK splits menu paths on '/' to any depth).
    //
    // Two characters need escaping in a name or a submenu, and each has its own
    // form. A literal slash is escaped with a backslash — "6\\/9" — which FLTK
    // strips while splitting the path. A literal ampersand is doubled — "Jazz &&
    // Blues" — because a lone '&' marks the next character as the mnemonic and is
    // swallowed when the item draws. Backslash does NOT work for '&': it is
    // stripped during the path split, leaving the bare '&' to become a mnemonic.
    const char* submenu;
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
    // Every scale sits in a family submenu; nothing is left at top level. Rows are
    // ordered by submenu so each menu builds from a contiguous run.

    // Classic. Chromatic lives here as the "no constraint" option — with a
    // selectable root it is genuinely useful, not just the absence of a scale.
    {"gyz07l", "Major",             7, {0, 2, 4,  5,  7,  9, 11}, true,  "Classic"},
    {"3zmfqr", "Minor (asc)",       7, {0, 2, 3,  5,  7,  9, 11}, true,  "Classic"},
    {"4psiem", "Minor (desc)",      7, {0, 2, 3,  5,  7,  8, 10}, true,  "Classic"},
    {"7hdzsf", "Minor (harmonic)",  7, {0, 2, 3,  5,  7,  8, 11}, true,  "Classic"},
    {"harmaj", "Harmonic major",    7, {0, 2, 4,  5,  7,  8, 11}, true,  "Classic"},
    {"m6skzq", "Major pent.",       5, {0, 2, 4,  7,  9,  0,  0}, true,  "Classic"},
    {"on64vt", "Minor pent.",       5, {0, 3, 5,  7, 10,  0,  0}, true,  "Classic"},
    {"neapmj", "Neapolitan major",  7, {0, 1, 3,  5,  7,  9, 11}, true,  "Classic"},
    {"neapmn", "Neapolitan minor",  7, {0, 1, 3,  5,  7,  8, 11}, true,  "Classic"},
    {"chrom1", "Chromatic",        12, {0, 1, 2,  3,  4,  5,  6,  7,  8,  9, 10, 11}, true, "Classic"},

    // Modes of the major scale. Ionian and Aeolian duplicate Major and Minor (desc)
    // on purpose — dropping them would leave holes in the mode sequence.
    {"modion", "Ionian",            7, {0, 2, 4,  5,  7,  9, 11}, true,  "Major modes"},
    {"q2vws4", "Dorian",            7, {0, 2, 3,  5,  7,  9, 10}, true,  "Major modes"},
    {"wjrku4", "Phrygian",          7, {0, 1, 3,  5,  7,  8, 10}, true,  "Major modes"},
    {"n19hmt", "Lydian",            7, {0, 2, 4,  6,  7,  9, 11}, true,  "Major modes"},
    {"cgkl5p", "Mixolydian",        7, {0, 2, 4,  5,  7,  9, 10}, true,  "Major modes"},
    {"g8suvb", "Aeolian",           7, {0, 2, 3,  5,  7,  8, 10}, true,  "Major modes"},
    {"pulps0", "Locrian",           7, {0, 1, 3,  5,  6,  8, 10}, true,  "Major modes"},

    // Symmetric — scales of limited transposition.
    {"ktcanc", "Whole tone",        6, {0, 2, 4,  6,  8, 10,  0}, true,  "Symmetric"},
    {"augmt6", "Augmented",         6, {0, 3, 4,  7,  8, 11,  0}, true,  "Symmetric"},
    {"oct8wh", "Whole-half",        8, {0, 2, 3,  5,  6,  8,  9, 11}, true, "Symmetric"},
    {"oct8hw", "Half-whole",        8, {0, 1, 3,  4,  6,  7,  9, 10}, true, "Symmetric"},

    // Jazz & blues. Altered, Lydian dominant and Half-diminished are modes of the
    // melodic minor, admitted on the strength of the jazz tradition behind them.
    {"blues6", "Blues",             6, {0, 3, 5,  6,  7, 10,  0}, true,  "Jazz && Blues"},
    {"bluesm", "Major blues",       6, {0, 2, 3,  4,  7,  9,  0}, true,  "Jazz && Blues"},
    {"altrd7", "Altered",           7, {0, 1, 3,  4,  6,  8, 10}, true,  "Jazz && Blues"},
    {"lyddom", "Lydian dominant",   7, {0, 2, 4,  6,  7,  9, 10}, true,  "Jazz && Blues"},
    {"hlfdim", "Half-diminished",   7, {0, 2, 3,  5,  6,  8, 10}, true,  "Jazz && Blues"},
    {"bebdom", "Bebop dominant",    8, {0, 2, 4,  5,  7,  9, 10, 11}, true, "Jazz && Blues"},
    {"bebmaj", "Bebop major",       8, {0, 2, 4,  5,  7,  8,  9, 11}, true, "Jazz && Blues"},
    {"bebdor", "Bebop dorian",      8, {0, 2, 3,  4,  5,  7,  9, 10}, true, "Jazz && Blues"},
    {"bebmin", "Bebop melodic min.",8, {0, 2, 3,  5,  7,  8,  9, 11}, true, "Jazz && Blues"},

    {"hunmaj", "Hungarian major",   7, {0, 3, 4,  6,  7,  9, 10}, true,  "World/Eastern Europe"},
    {"hunmin", "Hungarian minor",   7, {0, 2, 3,  6,  7,  8, 11}, true,  "World/Eastern Europe"},
    {"rommaj", "Romanian major",    7, {0, 1, 4,  6,  7,  9, 10}, true,  "World/Eastern Europe"},
    {"ukrdor", "Ukrainian dorian",  7, {0, 2, 3,  6,  7,  9, 10}, true,  "World/Eastern Europe"},

    // Byzantine is the double harmonic major (maqam Hijazkar); Phrygian Dominant is
    // maqam Hijaz. The quarter-tone maqamat (Rast, Bayati, Saba, Sikah) cannot be
    // represented in 12-TET and are deliberately absent.
    {"persia", "Persian",           7, {0, 1, 4,  5,  6,  8, 11}, true,  "World/Middle East"},
    {"byzant", "Byzantine",         7, {0, 1, 4,  5,  7,  8, 11}, true,  "World/Middle East"},
    {"phrydm", "Phrygian dominant", 7, {0, 1, 4,  5,  7,  8, 10}, true,  "World/Middle East"},

    // Japanese koto tunings, following Kostka & Payne. Sources disagree sharply here:
    // Burrows and Sachs/Slonimsky each assign these names to different rotations of
    // the same pentatonic pattern, so Hirajoshi in particular is spelled three ways
    // in the literature. Kostka & Payne is used throughout for consistency.
    {"hirajo", "Hirajoshi",         5, {0, 2, 3,  7,  8,  0,  0}, true,  "World/Japan"},
    {"iwato5", "Iwato",             5, {0, 1, 5,  6, 10,  0,  0}, true,  "World/Japan"},
    {"kumoi5", "Kumoi",             5, {0, 1, 5,  7,  9,  0,  0}, true,  "World/Japan"},
    {"insen5", "Insen",             5, {0, 1, 5,  7, 10,  0,  0}, true,  "World/Japan"},
    {"yoscal", "Yo",                5, {0, 2, 5,  7,  9,  0,  0}, true,  "World/Japan"},
    {"akebon", "Akebono",           5, {0, 2, 3,  7,  9,  0,  0}, true,  "World/Japan"},

    // The ten Hindustani thaats, complete. Seven duplicate pitch sets that already
    // appear above under Western names (noted per row) — kept so the family is whole
    // and reachable under the name a player working in that idiom would look for.
    {"bilava", "Bilaval",           7, {0, 2, 4,  5,  7,  9, 11}, true,  "World/India"},  // = Major
    {"khamaj", "Khamaj",            7, {0, 2, 4,  5,  7,  9, 10}, true,  "World/India"},  // = Mixolydian
    {"kafith", "Kafi",              7, {0, 2, 3,  5,  7,  9, 10}, true,  "World/India"},  // = Dorian
    {"asavar", "Asavari",           7, {0, 2, 3,  5,  7,  8, 10}, true,  "World/India"},  // = Minor (desc)
    {"bhairv", "Bhairavi",          7, {0, 1, 3,  5,  7,  8, 10}, true,  "World/India"},  // = Phrygian
    {"bhairo", "Bhairav",           7, {0, 1, 4,  5,  7,  8, 11}, true,  "World/India"},  // = Byzantine
    {"kalyan", "Kalyan",            7, {0, 2, 4,  6,  7,  9, 11}, true,  "World/India"},  // = Lydian
    {"marwa1", "Marwa",             7, {0, 1, 4,  6,  7,  9, 11}, true,  "World/India"},
    {"purvi1", "Purvi",             7, {0, 1, 4,  6,  7,  8, 11}, true,  "World/India"},
    {"todi01", "Todi",              7, {0, 1, 3,  6,  7,  8, 11}, true,  "World/India"},
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

// Semitone offset above the root for the n-th tone. A "pitch group" is one full
// cycle of the chord/scale's degrees — the block of rows the pattern editor draws
// between dark lines. Successive pitch groups are anchored exactly one real octave
// (12 semitones) apart, which keeps a note's root in the same octave before and
// after a chord/scale change and lets note conversions work upward from the root
// (see remapPatternNotes). A pitch group is NOT an octave of content: extended
// chords (9ths/11ths/13ths) reach above the octave, so their upper tones can
// overlap the next pitch group's lower tones — the row sequence is not strictly
// ascending for those chords, the accepted trade-off for keeping the root
// octave-stable.
inline int chordToneOffset(const ChordDef& def, int n)
{
    return def.intervals[n % def.size] + (n / def.size) * 12;
}

// Tone index of a stored pattern note. An ordinary note keeps its tone index in
// `row`; a bonus note keeps its pitch group there and its degree separately (that
// degree sits above the current chord's size), and the two recombine into the tone
// index the row's label shows — so a bonus note sounds exactly as labelled.
// Allocation-free, so it is safe to call from the RT thread.
inline int noteToneIndex(int row, bool bonus, int bonusDegree, int chordIndex)
{
    return bonus ? row * chordDefs[chordIndex].size + bonusDegree : row;
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
