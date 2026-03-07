#ifndef PLAYHEADRULER_HPP
#define PLAYHEADRULER_HPP

#include "itransport.hpp"
#include <FL/Fl_Widget.H>
#include <functional>

class PlayheadRuler : public Fl_Widget {
	ITransport* transport = nullptr;
	double bpm       = 120.0;
	int beatsPerBar  = 4;
	int numCols;
	int colWidth;

	std::function<void()> onSeek;

	void draw() override;
	int  handle(int event) override;

	double pixelToSeconds(int px) const;
	int    secondsToPixel(double secs) const;

	static void timerCb(void* data);

public:
	PlayheadRuler(int x, int y, int w, int h, int numCols, int colWidth);

	void setTransport(ITransport* t, double b, int bpb);
	void setOnSeek(std::function<void()> cb) { onSeek = cb; }
};

#endif
