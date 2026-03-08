#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

class Playhead : public ITimelineObserver {
	ITransport*         transport = nullptr;
	ObservableTimeline* obsTl     = nullptr;
	int                 numCols;
	int                 colWidth;
	Fl_Widget*          owner     = nullptr;

	static void timerCb(void* data);
	void tick();

	int    secondsToPixel(double secs) const;
	double pixelToSeconds(int px) const;

public:
	std::function<void()> onEndReached;

	Playhead(int numCols, int colWidth);
	~Playhead();

	void setTransport(ITransport* t, ObservableTimeline* tl);
	void setOwner(Fl_Widget* w) { owner = w; }

	void onTimelineChanged() override { if (owner) owner->redraw(); }

	void drawTriangle(int rulerX, int rulerY, int rulerH);
	void drawLine(int gridX, int gridY, int gridH);
	void seek(int mouseX, int rulerX);
	int  xOffset() const;
};

#endif
