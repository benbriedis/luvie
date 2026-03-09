#ifndef TIMELINE_HPP
#define TIMELINE_HPP

#include <string>
#include <vector>

struct Note {
	int   id;
	int   row;             // MIDI pitch (0-127) in pattern context; track index in song context
	float col;             // beat start position in pattern context; bar position in song context
	float length;          // in beats / bars
	float velocity    = 0.0f;
	float startOffset = 0.0f;  // beat offset into the pattern where playback starts (song context)
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
	std::vector<Note> notes;
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
	std::vector<PatternInstance> patterns;
};

struct Timeline {
	std::vector<BpmMarker>     bpms;
	std::vector<TimeSigMarker> timeSigs;
	std::vector<Pattern>       patterns;  // pattern definitions
	std::vector<Track>         tracks;
};

#endif
