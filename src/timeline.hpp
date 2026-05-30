#ifndef TIMELINE_HPP
#define TIMELINE_HPP

#include <set>
#include <string>
#include <vector>

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

struct BpmMarker {
	int   bar;
	float bpm;
};

struct TimeSigMarker {
	int bar;
	int top;
	int bottom;
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
	std::string outputInstrumentName;  // empty = no routing assigned
	std::string name;                  // display name; empty = auto ("InstrumentName N")
	int timeSigTop    = 4;
	int timeSigBottom = 4;
};

struct PatternInstance {
	int   id;
	int   patternId;
	float startBar;
	float length;          // instance length in bars
	float startOffset = 0.0f;  // beat offset into the pattern to start playing from
};

// A single row of pattern instances within a Track (instrument).
// Phase 1: each Track has exactly one Lane.
struct Lane {
	int id;
	int patternId = 0;   // the pattern displayed in the pattern editor for this lane
	std::vector<PatternInstance> patterns;
};

struct Track {
	int         id;
	std::string label;
	bool        solo         = false;
	bool        mute         = false;
	bool        stackedLanes = false;
	std::vector<Lane> lanes;
};

// Discriminates the three kinds of visible row in the song grid.
enum class RowKind { Lane, Header, Param };

// One entry per visible row; determines display order.
// Lane  → id is a Lane ID (holds pattern instances)
// Header→ id is a Track ID (instrument name row, holds no instances)
// Param → id is a ParamLane ID (automation lane)
struct RowRef {
	RowKind kind;
	int     id;
};

struct Timeline {
	std::vector<BpmMarker>     bpms;
	std::vector<TimeSigMarker> timeSigs;
	std::vector<Pattern>       patterns;  // pattern definitions
	std::vector<Track>         tracks;
	std::vector<ParamLane>     paramLanes;
	std::vector<RowRef>        rowOrder;  // interleaved display order
	int                        selectedTrackIndex = -1;
	int                        selectedLaneId     = -1;

	int patternIdForSelectedLane() const {
		if (selectedTrackIndex < 0 || selectedTrackIndex >= (int)tracks.size()) return 0;
		const auto& track = tracks[selectedTrackIndex];
		if (track.lanes.empty()) return 0;
		for (const auto& l : track.lanes)
			if (l.id == selectedLaneId) return l.patternId;
		return track.lanes[0].patternId;
	}
};

#endif
