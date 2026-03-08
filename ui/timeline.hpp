#ifndef TIMELINE_HPP
#define TIMELINE_HPP

#include <vector>

struct BpmMarker {
	int   bar;
	float bpm;
};

struct TimeSigMarker {
	int bar;
	int top;
	int bottom;
};

struct Timeline {
	std::vector<BpmMarker>    bpms;
	std::vector<TimeSigMarker> timeSigs;
};

#endif
