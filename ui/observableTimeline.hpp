#ifndef OBSERVABLE_TIMELINE_HPP
#define OBSERVABLE_TIMELINE_HPP

#include "timeline.hpp"
#include <vector>

class ITimelineObserver {
public:
	virtual ~ITimelineObserver() = default;
	virtual void onTimelineChanged() = 0;
};

class ObservableTimeline {
public:
	ObservableTimeline(float initBpm, int initTop, int initBottom);

	void addObserver(ITimelineObserver* o);
	void removeObserver(ITimelineObserver* o);

	const Timeline& get() const { return data; }

	// BPM
	void  setBpm(int bar, float bpm);        // update existing marker or add new one
	void  removeBpm(int bar);                // bar 0 is ignored (fixed)
	void  moveBpmMarker(int fromBar, int toBar);
	float bpmAt(int bar) const;              // effective BPM at or before bar

	// Time signature
	void setTimeSig(int bar, int top, int bottom);
	void removeTimeSig(int bar);
	void moveTimeSigMarker(int fromBar, int toBar);
	void timeSigAt(int bar, int& top, int& bottom) const;

	// Time conversion — integrates over BPM/time-sig segments
	double barToSeconds(float bar) const;
	float  secondsToBar(double secs) const;
	void   secondsToBarBeat(double secs, int& bar, int& beat) const;

private:
	struct TimeSegment {
		int   bar;
		float bpm;
		int   beatsPerBar;
	};
	std::vector<TimeSegment> buildSegments() const;
	Timeline data;
	std::vector<ITimelineObserver*> observers;

	void notify();
	void sortBpms();
	void sortTimeSigs();
};

#endif
