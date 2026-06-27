#ifndef TIMELINE_HPP
#define TIMELINE_HPP

#include "patternData.hpp"   // Note, DrumNote, ParamLane, Pattern, PatternType
#include <string>
#include <vector>

// Song-level arrangement: how patterns are placed on tracks/lanes over time,
// plus tempo/time-signature markers and the overall Timeline aggregate. The
// per-pattern content structs live in patternEditors/patternData.hpp.

struct Instrument {
    int         id;
    std::string name;
    bool        isDrum = false;
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
	int  id;
	int  instrumentId    = 0;
	bool solo            = false;
	bool mute            = false;
	bool stackedLanes    = false;
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
	std::vector<Instrument>    instruments;
	std::vector<ParamLane>     paramLanes;
	std::vector<RowRef>        rowOrder;  // interleaved display order
	int                        selectedTrackIndex = -1;
	int                        selectedLaneId     = -1;

	std::string instrumentName(int id) const {
		for (const auto& i : instruments)
			if (i.id == id) return i.name;
		return {};
	}

	bool instrumentIsDrum(int id) const {
		for (const auto& i : instruments)
			if (i.id == id) return i.isDrum;
		return false;
	}

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
