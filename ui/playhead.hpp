#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

class Playhead : public ITimelineObserver {
	ITransport*         transport    = nullptr;
	ObservableTimeline* obsTl        = nullptr;
	int                 numCols;
	int                 colWidth;
	Fl_Widget*          owner        = nullptr;
	int                 patternTrack = -1;  // >= 0: beat-relative view of that track

	static void timerCb(void* data);
	void tick();
	bool isInPattern(float currentBar) const;

	int    secondsToPixel(double secs) const;
	double pixelToSeconds(int px) const;

public:
	std::function<void()> onEndReached;

	Playhead(int numCols, int colWidth);
	~Playhead();

	void setTransport(ITransport* t, ObservableTimeline* tl);
	void setOwner(Fl_Widget* w) { owner = w; }
	void setPatternTrack(int track) { patternTrack = track; }

	void onTimelineChanged() override { if (owner) owner->redraw(); }

	void  drawTriangle(int rulerX, int rulerY, int rulerH);
	void  drawLine(int gridX, int gridY, int gridH);
	void  seek(int mouseX, int rulerX);
	int   xOffset() const;
	float currentBar() const;
};

#endif
