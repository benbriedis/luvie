#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

class Playhead {
	ITransport* transport = nullptr;
	double bpm      = 120.0;
	int beatsPerBar = 4;
	int numCols;
	int colWidth;
	Fl_Widget* owner = nullptr;

	static void timerCb(void* data);
	void tick();

	int    secondsToPixel(double secs) const;
	double pixelToSeconds(int px) const;

public:
	std::function<void()> onEndReached;

	Playhead(int numCols, int colWidth);

	void setTransport(ITransport* t, double b, int bpb);
	void setOwner(Fl_Widget* w) { owner = w; }

	// Draw the downward-pointing triangle in the ruler area
	void drawTriangle(int rulerX, int rulerY, int rulerH);

	// Draw the vertical line — called from MyGrid::draw()
	void drawLine(int gridX, int gridY, int gridH);

	// Seek to the position corresponding to mouseX within the ruler
	void seek(int mouseX, int rulerX);

	// Pixel offset from the ruler's left edge to the triangle centre
	int xOffset() const;
};

#endif
