#ifndef PATTERN_DATA_HPP
#define PATTERN_DATA_HPP

#include "timeSettings.hpp"  // BeatUnit
#include <set>
#include <string>
#include <vector>

// Contents of a single pattern: the notes, drum hits and automation that the
// pattern editors manipulate. The song-level arrangement structs that aggregate
// these (Pattern instances, Lanes, Tracks, Timeline) live in songEditor/timeline.hpp.

enum class PatternType { STANDARD = 0, DRUM = 1, PIANOROLL = 2 };

struct DrumNote {
	int   id;
	int   note;        // MIDI note number (0–127)
	float beat;
	float velocity = 0.8f;
};

struct Note {
	int   id;
	int   row;             // abs_row in current chord encoding; when disabled: stores octave only
	float beat;
	float length;
	float velocity       = 0.0f;
	bool  disabled       = false;
	int   disabledDegree = -1;  // degree at time of disabling (>= chord size); -1 when enabled
};

struct ParamPoint {
	int   id;
	float beat;
	int   value   = 63;    // 0-127 for CC, 0-16383 for pitch bend
	bool  anchor  = false; // can't be deleted or moved horizontally
};

struct ParamLane {
	int                      id;
	std::string              type;    // "Pitch", "Modulation", etc.
	std::vector<ParamPoint>  points;  // sorted by beat
	int                      instrumentId = 0;  // owning instrument; routes to its port only
};

struct Pattern {
	int   id;
	float lengthBeats;
	PatternType            type = PatternType::STANDARD;
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
	int  octaveOffset = 0;   // -1, 0 or 1; added to every note's octave at playback
	int  divisions = 0;      // divChoice index; 0 = None (kDivisionsDefault in patternPanel.cpp)
	bool snapEnabled = true; // snap new notes and resized edges to the divisions
	int  zoom      = 1;      // zoomChoice index; 1 = x2 (kZoomDefault in patternPanel.cpp)
};

#endif
