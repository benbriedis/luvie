#include "playhead.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>

static constexpr Fl_Color headColor = 0xEF444400;  // red

Playhead::Playhead(int numCols, int colWidth)
	: numCols(numCols), colWidth(colWidth)
{}

Playhead::~Playhead()
{
	Fl::remove_timeout(timerCb, this);
	if (obsTl) obsTl->removeObserver(this);
}

void Playhead::setTransport(ITransport* t, ObservableTimeline* tl)
{
	Fl::remove_timeout(timerCb, this);
	if (obsTl) obsTl->removeObserver(this);

	transport = t;
	obsTl     = tl;

	if (obsTl) obsTl->addObserver(this);

	Fl::add_timeout(0.1, timerCb, this);
}

void Playhead::onTimelineChanged()
{
	if (transport && obsTl) {
		double endSecs = obsTl->barToSeconds((float)numCols);
		transport->seek(std::clamp(obsTl->barToSeconds(positionBars), 0.0, endSecs));
	}
	if (owner) owner->redraw();
}

void Playhead::timerCb(void* data)
{
	auto* self = static_cast<Playhead*>(data);
	self->tick();

	double interval = 0.1;
	if (self->transport && self->transport->isPlaying() && self->obsTl) {
		float  curBar     = self->obsTl->secondsToBar(self->transport->position());
		float  bpm        = self->obsTl->bpmAt((int)curBar);
		int    top, bottom;
		self->obsTl->timeSigAt((int)curBar, top, bottom);
		double pxPerSec   = (double)self->colWidth * bpm / 60.0 / top;
		interval = std::clamp(1.0 / pxPerSec, 0.016, 0.05);
	}
	Fl::repeat_timeout(interval, timerCb, data);
}

void Playhead::tick()
{
	// Only gate the transport in normal mode; pattern-view playheads are display-only.
	if (patternTrack < 0 && transport && transport->isPlaying() && obsTl) {
		double endSecs = obsTl->barToSeconds((float)numCols);
		if (transport->position() >= endSecs) {
			transport->pause();
			if (onEndReached) onEndReached();
		}
	}
	if (transport && obsTl)
		positionBars = obsTl->secondsToBar(transport->position());
	if (owner) owner->redraw();
}

bool Playhead::isInPattern(float currentBar) const
{
	if (patternTrack < 0 || !obsTl) return true;
	const auto& tracks = obsTl->get().tracks;
	if (patternTrack >= (int)tracks.size()) return false;
	for (auto& inst : tracks[patternTrack].patterns)
		if (currentBar >= inst.startBar && currentBar < inst.startBar + inst.length)
			return true;
	return false;
}

int Playhead::secondsToPixel(double secs) const
{
	if (!obsTl) return 0;
	float currentBar = obsTl->secondsToBar(secs);

	if (patternTrack >= 0) {
		// Beat-relative position within the active pattern instance on patternTrack.
		const auto& tracks = obsTl->get().tracks;
		if (patternTrack >= (int)tracks.size()) return 0;
		for (auto& inst : tracks[patternTrack].patterns) {
			if (currentBar >= inst.startBar && currentBar < inst.startBar + inst.length) {
				int top, bottom;
				obsTl->timeSigAt((int)inst.startBar, top, bottom);
				float raw        = std::fmod(inst.startOffset + (currentBar - inst.startBar) * top, (float)numCols);
			float beatOffset = raw < 0.0f ? raw + (float)numCols : raw;
				int px = (int)(beatOffset * colWidth);
				return std::clamp(px, 0, numCols * colWidth - 2);
			}
		}
		return 0;
	}

	int px = (int)(currentBar * colWidth);
	return std::clamp(px, 0, numCols * colWidth - 2);
}

double Playhead::pixelToSeconds(int px) const
{
	if (!obsTl) return 0.0;
	return obsTl->barToSeconds((float)px / colWidth);
}

void Playhead::drawTriangle(int rulerX, int rulerY, int rulerH)
{
	if (!transport || !obsTl) return;
	float currentBar = obsTl->secondsToBar(transport->position());
	if (!isInPattern(currentBar)) return;
	int px  = rulerX + secondsToPixel(transport->position());
	int tw  = 11;
	int top = rulerY + 2;
	int tip = rulerY + rulerH - 3;
	fl_color(headColor);
	fl_polygon(px - tw/2, top, px + tw/2, top, px, tip);
}

void Playhead::drawLine(int gridX, int gridY, int gridH)
{
	if (!transport || !obsTl) return;
	float currentBar = obsTl->secondsToBar(transport->position());
	if (!isInPattern(currentBar)) return;
	int px = gridX + secondsToPixel(transport->position());
	fl_color(headColor);
	fl_line(px, gridY, px, gridY + gridH);
}

int Playhead::xOffset() const
{
	return transport ? secondsToPixel(transport->position()) : 0;
}

float Playhead::currentBar() const
{
	if (!transport || !obsTl) return 0.0f;
	return obsTl->secondsToBar(transport->position());
}

void Playhead::seek(int mouseX, int rulerX)
{
	if (!transport || !obsTl) return;
	double secs    = pixelToSeconds(mouseX - rulerX);
	double endSecs = obsTl->barToSeconds((float)numCols);
	secs = std::clamp(secs, 0.0, endSecs);
	transport->seek(secs);
	positionBars = obsTl->secondsToBar(secs);
}
