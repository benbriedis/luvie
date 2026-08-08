// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef TIMELINE_HPP
#define TIMELINE_HPP

#include "patternData.hpp"   // Note, DrumNote, ParamLane, Pattern, PatternType
#include "timeSettings.hpp"  // BeatUnit
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

// Tempo in beats per minute, where a beat is the BeatUnit of the time signature
// in force. Crotchets per minute — what the timing math runs on — is derived:
// see ObservableSong::cpmAt().
//
// A Linear marker is a ramp rather than a point: the tempo goes from bpm to
// endBpm over the bars [bar, bar + lengthBars), stepping once per beat, and
// endBpm is the tempo in force from bar + lengthBars onwards. The ramp therefore
// ends flush with the end of bar (bar + lengthBars - 1). The three ramp fields
// are ignored — and not written to the song file — when curve is Immediate.
//
// A ramp's bpm is derived, not chosen: it is the tempo already in force where
// the ramp starts, so ObservableSong::sortBpms() recomputes it from the previous
// marker and the song file leaves it out. The one exception is the very first
// marker, which has no earlier tempo to inherit and so owns its bpm outright.
struct BpmMarker {
	int   bar;
	float bpm;
	timeSettings::TempoCurve curve = timeSettings::TempoCurve::Immediate;
	int   lengthBars = 1;      // Linear only; >= 1
	float endBpm     = 0.0f;   // Linear only

	bool  isRamp()    const { return curve == timeSettings::TempoCurve::Linear; }
	// One past the last bar the ramp covers; == bar for an immediate marker.
	int   rampEndBar() const { return isRamp() ? bar + lengthBars : bar; }
};

struct TimeSigMarker {
	int bar;
	int top;
	int bottom;
	timeSettings::BeatUnit beat = timeSettings::beatUnitDefault;
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
	std::vector<int>  loopLanes;  // lane IDs in loop-editor order (independent of lanes[])
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
	std::vector<RowRef>        rowOrder;  // interleaved display order (song editor)
	std::vector<int>           loopOrder; // track IDs in loop editor display order
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
