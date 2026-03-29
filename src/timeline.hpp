#ifndef TIMELINE_HPP
#define TIMELINE_HPP

#include <string>
#include <vector>

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
	int         patternId = 0;   // the pattern displayed in the pattern editor for this track
	std::vector<PatternInstance> patterns;
};

struct Timeline {
	std::vector<BpmMarker>     bpms;
	std::vector<TimeSigMarker> timeSigs;
	std::vector<Pattern>       patterns;  // pattern definitions
	std::vector<Track>         tracks;
	int                        selectedTrackIndex = -1;
};

#endif
