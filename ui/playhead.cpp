#include "playhead.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>

static constexpr Fl_Color rulerBg     = 0xFEFCE800;  // pale yellow
static constexpr Fl_Color rulerBorder = 0xD1D5DB00;  // gray-300
static constexpr Fl_Color headColor   = 0xEF444400;  // red

Playhead::Playhead(int x, int y, int w, int h, int numCols, int colWidth)
	: Fl_Widget(x, y, w, h), numCols(numCols), colWidth(colWidth)
{}

void Playhead::setTransport(ITransport* t, double b, int bpb) {
	Fl::remove_timeout(timerCb, this);
	transport   = t;
	bpm         = b;
	beatsPerBar = bpb;
	Fl::add_timeout(0.1, timerCb, this);
}

void Playhead::timerCb(void* data) {
	auto* self = static_cast<Playhead*>(data);
	self->tick();
	double interval = 0.1;
	if (self->transport && self->transport->isPlaying()) {
		double pxPerSec = (double)self->colWidth * self->bpm / 60.0 / self->beatsPerBar;
		interval = std::clamp(1.0 / pxPerSec, 0.016, 0.05);
	}
	Fl::repeat_timeout(interval, timerCb, data);
}

void Playhead::tick() {
	if (transport && transport->isPlaying()) {
		double endSecs = (double)numCols * beatsPerBar / bpm * 60.0;
		if (transport->position() >= endSecs) {
			transport->pause();
			if (onEndReached) onEndReached();
		}
	}
	redraw();
	if (linkedGrid) linkedGrid->redraw();
}

double Playhead::pixelToSeconds(int px) const {
	return (double)px / colWidth * beatsPerBar / bpm * 60.0;
}

int Playhead::secondsToPixel(double secs) const {
	int px = (int)(secs * bpm / 60.0 / beatsPerBar * colWidth);
	return std::clamp(px, 0, numCols * colWidth - 2);
}

void Playhead::draw() {
	fl_color(rulerBg);
	fl_rectf(x(), y(), w(), h());

	fl_color(rulerBorder);
	fl_line(x(), y() + h() - 1, x() + w() - 1, y() + h() - 1);

	if (transport) {
		int px  = x() + secondsToPixel(transport->position());
		int tw  = 11;
		int top = y() + 2;
		int tip = y() + h() - 3;
		fl_color(headColor);
		fl_polygon(px - tw/2, top, px + tw/2, top, px, tip);
	}
}

void Playhead::drawLine(int gridX, int gridY, int gridH) {
	if (!transport) return;
	int px = gridX + secondsToPixel(transport->position());
	fl_color(headColor);
	fl_line_style(FL_SOLID, 2);
	fl_line(px, gridY, px, gridY + gridH);
	fl_line_style(0);
}

int Playhead::handle(int event) {
	switch (event) {
	case FL_PUSH:
	case FL_DRAG: {
		if (!transport) return 1;
		int px = Fl::event_x() - x();
		double secs    = pixelToSeconds(px);
		double endSecs = (double)numCols * beatsPerBar / bpm * 60.0;
		secs = std::clamp(secs, 0.0, endSecs);
		transport->seek(secs);
		redraw();
		if (linkedGrid) linkedGrid->redraw();
		return 1;
	}
	case FL_RELEASE:
		return 1;
	case FL_ENTER:
		window()->cursor(FL_CURSOR_WE);
		return 1;
	case FL_LEAVE:
		window()->cursor(FL_CURSOR_DEFAULT);
		return 0;
	default:
		return 0;
	}
}
