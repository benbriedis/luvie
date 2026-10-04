// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PATTERN_DATA_HPP
#define PATTERN_DATA_HPP

#include "chords.hpp"        // noteToneIndex, rowToMidi
#include "timeSettings.hpp"  // BeatUnit
#include <algorithm>
#include <iterator>   // std::size
#include <set>
#include <string>
#include <vector>

// Contents of a single pattern: the notes, drum hits and automation that the
// pattern editors manipulate. The song-level arrangement structs that aggregate
// these (Pattern instances, Lanes, Tracks, Timeline) live in songEditor/timeline.hpp.

enum class PatternType { HARMONY = 0, DRUM = 1, PIANOROLL = 2 };

// Short tag naming a pattern's kind, drawn faintly under the pattern name in the
// song editor's track labels and in the loop editor's pattern blocks.
inline const char* patternKindLabel(PatternType type)
{
	switch (type) {
		case PatternType::PIANOROLL: return "pianoroll";
		case PatternType::DRUM:      return "drums";
		default:                     return "harmony";
	}
}

struct DrumNote {
	int   id;
	int   note;        // MIDI note number (0–127)
	float beat;
	float velocity = 0.8f;
};

struct Note {
	int   id;
	int   row;          // abs_row in current chord encoding; for a bonus note: the pitch group only
	float beat;
	float length;
	float velocity    = 0.0f;
	// A bonus note is one whose degree no longer fits the pattern's chord — it sits
	// in a greyed "bonus row" above the chord's degrees within its pitch group, and
	// keeps its own degree so a later switch back to a bigger chord restores it.
	// Bonus notes play like any other; see noteToneIndex() in chords.hpp.
	bool  bonus       = false;
	int   bonusDegree = -1;  // degree the note kept (>= chord size); -1 when not a bonus note
};

struct ParamPoint {
	int   id;
	float beat;
	int   value   = 63;    // 0-127 for CC, 0-16383 for pitch bend
	bool  anchor  = false; // can't be deleted or moved horizontally
};

struct ParamLane {
	int                      id;
	std::string              type;    // the parameter it automates: "Pitch", "Cutoff", ... (see paramDefs.hpp)
	std::vector<ParamPoint>  points;  // sorted by beat
	int                      instrumentId = 0;  // owning instrument; routes to its port only
};

struct Pattern {
	int   id;
	float lengthBeats;
	PatternType            type = PatternType::HARMONY;
	std::vector<Note>      notes;
	std::vector<DrumNote>  drumNotes;
	std::set<int>          drumSolo;   // MIDI notes to solo (empty = all play)
	std::set<int>          drumMute;   // MIDI notes to silence
	std::vector<ParamLane> paramLanes;
	int  instrumentId = 0;             // 0 = no routing assigned
	std::string name;                  // display name; empty = auto ("InstrumentName N")
	int timeSigTop    = 4;
	int timeSigBottom = 4;
	timeSettings::BeatUnit beat = timeSettings::beatUnitDefault;
	int  rootPitch = 0;      // base note index (matches rootChoice)
	std::string chordHash;   // stable ChordDef hash; empty = "major" (see chords.hpp)
	bool useSharp  = false;  // #/b display spelling
	int  divisions = 0;      // index into kPatternDivisors; 0 = None
	bool snapEnabled = true; // snap new notes and resized edges to the divisions
	int  zoomPct   = 200;    // horizontal zoom, % of the x1 column width (kZoomPercents in patternPanel.cpp)
};

// The beat subdivisions the Div control offers, indexed by Pattern::divisions.
inline constexpr int kPatternDivisors[]  = { 1, 2, 3, 4, 5, 6, 7 };
inline constexpr int kDivisionsDefault   = 0;  // None

// Parts per beat for a Pattern::divisions index, falling back to the default for
// an index out of range.
inline int patternDivisor(int divisionsIdx)
{
	if (divisionsIdx < 0 || divisionsIdx >= (int)std::size(kPatternDivisors))
		divisionsIdx = kDivisionsDefault;
	return kPatternDivisors[divisionsIdx];
}

// The Snap quantum in beats for a pattern's own Div and Snap settings, or 0 when
// Snap is off. What the panel sends the editors for the pattern on screen, and what
// recording into a pattern quantises to whether it is on screen or not.
inline float patternSnapBeats(const Pattern& p)
{
	return p.snapEnabled ? 1.0f / (float)patternDivisor(p.divisions) : 0.0f;
}

// MIDI pitch a stored note sounds at. A pianoroll pattern keeps the MIDI note
// number itself in `row`; a harmony pattern keeps a chord-tone index that only
// becomes a pitch once resolved against the pattern's root and chord. Every
// playback path (Sequencer's RT snapshot, the soft Playhead output) must go
// through here, else pianoroll rows get read as chord tones and land far above
// the top of the MIDI range. chordIndex is pre-resolved from pat.chordHash by
// the caller (chordIndexForHash) as it is loop-invariant. Allocation-free, so it
// is safe to call from the RT thread.
inline int patternNoteMidi(const Pattern& pat, const Note& note, int chordIndex)
{
	if (pat.type == PatternType::PIANOROLL)
		return std::clamp(note.row, 0, 127);
	int tone = noteToneIndex(note.row, note.bonus, note.bonusDegree, chordIndex);
	return rowToMidi(tone, pat.rootPitch, chordIndex);
}

#endif
