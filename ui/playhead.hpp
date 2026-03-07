#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

class Playhead : public Fl_Widget {
	ITransport* transport = nullptr;
	double bpm      = 120.0;
	int beatsPerBar = 4;
	int numCols;
	int colWidth;

	Fl_Widget* linkedGrid = nullptr;

	static void timerCb(void* data);
	void tick();

	double pixelToSeconds(int px) const;
	int    secondsToPixel(double secs) const;

	void draw() override;
	int  handle(int event) override;

public:
	std::function<void()> onEndReached;

	Playhead(int x, int y, int w, int h, int numCols, int colWidth);

	void setTransport(ITransport* t, double b, int bpb);
	void setLinkedGrid(Fl_Widget* grid) { linkedGrid = grid; }

	// Called from the grid's draw() to paint the vertical line
	void drawLine(int gridX, int gridY, int gridH);
};

#endif
