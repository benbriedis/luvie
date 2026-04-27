#ifndef TIMELINE_HPP
#define TIMELINE_HPP

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
	int   pitch;           // abs_row in current chord encoding; when disabled: stores octave only
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

struct Pattern {
	int   id;
	float lengthBeats;
	PatternType       type = PatternType::STANDARD;
	std::vector<Note>     notes;
	std::vector<DrumNote> drumNotes;
	std::string outputInstrumentName;  // empty = no routing assigned
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

struct Track {
	int         id;
	std::string label;
	int         patternId = 0;   // the pattern displayed in the pattern editor for this track
	std::vector<PatternInstance> patterns;
};

struct ParamPoint {
	int   id;
	float beat;
	int   value   = 63;    // 0-127
	bool  anchor  = false; // can't be deleted or moved horizontally
};

struct ParamLane {
	int                      id;
	std::string              type;    // "Pitch", "Modulation", etc.
	std::vector<ParamPoint>  points;  // sorted by beat
};

struct Timeline {
	std::vector<BpmMarker>     bpms;
	std::vector<TimeSigMarker> timeSigs;
	std::vector<Pattern>       patterns;  // pattern definitions
	std::vector<Track>         tracks;
	std::vector<ParamLane>     paramLanes;
	int                        selectedTrackIndex = -1;
};

#endif
