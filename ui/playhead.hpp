#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <FL/Fl_Widget.H>
#include <functional>
#include <string>

class Playhead : public ITimelineObserver {
	ITransport*         transport    = nullptr;
	ObservableTimeline* obsTl        = nullptr;
	int                 numCols;
	int                 colWidth;
	Fl_Widget*          owner        = nullptr;
	int                 patternTrack = -1;  // >= 0: beat-relative view of that track

	bool  verbose      = false;
	float lastPosition = 0.0f;

	static void timerCb(void* data);
	void tick();
	bool isInPattern(float bars) const;
	void checkVerboseNotes(float prevPos, float curPos);

	int   barsToPixel(float bars) const;
	float pixelToBars(int px)     const;

public:
	std::function<void()>        onEndReached;
	std::function<std::string(int)> pitchName;   // optional: pitch index → "E4" etc.

	Playhead(int numCols, int colWidth);
	~Playhead();

	void setTransport(ITransport* t, ObservableTimeline* tl);
	void setOwner(Fl_Widget* w)   { owner   = w; }
	void setVerbose(bool v)       { verbose = v; }
	void setPatternTrack(int track) { patternTrack = track; }

	void onTimelineChanged() override;

	void  drawTriangle(int rulerX, int rulerY, int rulerH);
	void  drawLine(int gridX, int gridY, int gridH);
	void  seek(int mouseX, int rulerX);
	int   xOffset() const;
	float currentBar() const;
};

#endif
