#include "playhead.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <cstdio>

static constexpr Fl_Color headColor = 0xEF444400;  // red

Playhead::Playhead(int numCols, int colWidth)
	: numCols(numCols), colWidth(colWidth)
{}

Playhead::~Playhead()
{
	Fl::remove_timeout(timerCb, this);
	swapObserver(obsTl, nullptr, this);
}

void Playhead::setTransport(ITransport* t, ObservableTimeline* tl)
{
	Fl::remove_timeout(timerCb, this);
	transport = t;
	swapObserver(obsTl, tl, this);
	Fl::add_timeout(0.1, timerCb, this);
}

void Playhead::onTimelineChanged()
{
	// Re-anchor the transport so bar position stays stable across BPM/timeSig changes.
	if (transport) {
		float bars = transport->position();
		transport->seek(std::clamp(bars, 0.0f, (float)numCols));
	}
	if (owner && owner->visible_r()) owner->redraw();
}

void Playhead::timerCb(void* data)
{
	auto* self = static_cast<Playhead*>(data);
	self->tick();

	double interval = 0.1;
	if (self->transport && self->transport->isPlaying() && self->obsTl) {
		float curBar = self->transport->position();
		float bpm    = self->obsTl->bpmAt((int)curBar);
		int   top, bottom;
		self->obsTl->timeSigAt((int)curBar, top, bottom);
		double pxPerSec = (double)self->colWidth * bpm / 60.0 / top;
		interval = std::clamp(1.0 / pxPerSec, 0.016, 0.05);
	}
	Fl::repeat_timeout(interval, timerCb, data);
}

void Playhead::tick()
{
	// Only gate the transport in normal mode; pattern-view playheads are display-only.
	if (patternTrack < 0 && transport && transport->isPlaying()) {
		float curPos = transport->position();
		if (verbose && obsTl && curPos >= lastPosition)
			checkVerboseNotes(lastPosition, curPos);
		lastPosition = curPos;
		if (curPos >= (float)numCols) {
			transport->pause();
			if (onEndReached) onEndReached();
		}
	} else if (transport) {
		// Keep lastPosition in sync while paused so resuming doesn't re-fire past notes.
		lastPosition = transport->position();
	}
	if (owner && owner->visible_r()) owner->redraw();
}

void Playhead::checkVerboseNotes(float prevPos, float curPos)
{
	const Timeline& tl = obsTl->get();
	for (const Track& track : tl.tracks) {
		for (const PatternInstance& inst : track.patterns) {
			if (inst.startBar + inst.length <= prevPos) continue;
			if (inst.startBar >= curPos) continue;

			const Pattern* pat = obsTl->patternForInstance(inst.id);
			if (!pat || pat->notes.empty() || pat->lengthBeats <= 0.0f) continue;

			int top, bottom;
			obsTl->timeSigAt((int)inst.startBar, top, bottom);
			float beatsPerBar = (float)top;

			// Convert the song-bar window to a pattern-beat window for this instance.
			float windowStart = std::max(prevPos, inst.startBar);
			float windowEnd   = std::min(curPos,  inst.startBar + inst.length);
			float beatStart   = inst.startOffset + (windowStart - inst.startBar) * beatsPerBar;
			float beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * beatsPerBar;

			for (const Note& note : pat->notes) {
				if (note.disabled) continue;
				// Find the first firing of this note (accounting for pattern looping)
				// at or after beatStart.
				float len      = pat->lengthBeats;
				float cycles   = std::floor((beatStart - note.beat) / len);
				float firstFire = note.beat + cycles * len;
				if (firstFire < beatStart) firstFire += len;

				if (firstFire < beatEnd) {
					float songBar  = inst.startBar + (firstFire - inst.startOffset) / beatsPerBar;
					int   bar      = (int)songBar + 1;
					int   beat     = (int)((songBar - std::floor(songBar)) * beatsPerBar) + 1;
					std::string name = pitchName ? pitchName(note.pitch)
					                             : std::to_string(note.pitch);
					printf("[verbose] bar %d beat %d | track \"%s\"  note=%-4s  beat=%.2f  len=%.2f\n",
					       bar, beat, track.label.c_str(), name.c_str(), note.beat, note.length);
				}
			}
		}
	}
}

bool Playhead::isInPattern(float bars) const
{
	if (patternTrack < 0 || !obsTl) return true;
	const auto& tracks = obsTl->get().tracks;
	if (patternTrack >= (int)tracks.size()) return false;
	for (auto& inst : tracks[patternTrack].patterns)
		if (bars >= inst.startBar && bars < inst.startBar + inst.length)
			return true;
	return false;
}

int Playhead::barsToPixel(float bars) const
{
	if (patternTrack >= 0) {
		if (!obsTl) return 0;
		const auto& tracks = obsTl->get().tracks;
		if (patternTrack >= (int)tracks.size()) return 0;
		for (auto& inst : tracks[patternTrack].patterns) {
			if (bars >= inst.startBar && bars < inst.startBar + inst.length) {
				int top, bottom;
				obsTl->timeSigAt((int)inst.startBar, top, bottom);
				float raw        = std::fmod(inst.startOffset + (bars - inst.startBar) * top, (float)numCols);
				float beatOffset = raw < 0.0f ? raw + (float)numCols : raw;
				int px = (int)(beatOffset * colWidth);
				return std::clamp(px, 0, numCols * colWidth - 2);
			}
		}
		return 0;
	}

	int px = (int)(bars * colWidth);
	return std::clamp(px, 0, numCols * colWidth - 2);
}

float Playhead::pixelToBars(int px) const
{
	return (float)px / colWidth;
}

void Playhead::drawTriangle(int rulerX, int rulerY, int rulerH)
{
	if (!transport) return;
	float bars = transport->position();
	if (!isInPattern(bars)) return;
	int px  = rulerX + barsToPixel(bars);
	int tw  = 11;
	int top = rulerY + 2;
	int tip = rulerY + rulerH - 3;
	fl_color(headColor);
	fl_polygon(px - tw/2, top, px + tw/2, top, px, tip);
}

void Playhead::drawLine(int gridX, int gridY, int gridH)
{
	if (!transport) return;
	float bars = transport->position();
	if (!isInPattern(bars)) return;
	int px = gridX + barsToPixel(bars);
	fl_color(headColor);
	fl_line(px, gridY, px, gridY + gridH);
}

int Playhead::xOffset() const
{
	return transport ? barsToPixel(transport->position()) : 0;
}

float Playhead::currentBar() const
{
	return transport ? transport->position() : 0.0f;
}

void Playhead::seek(int mouseX, int rulerX)
{
	if (!transport) return;
	float bars = pixelToBars(mouseX - rulerX);
	transport->seek(std::clamp(bars, 0.0f, (float)numCols));
}
