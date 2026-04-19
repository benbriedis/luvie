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
	if (transport) {
		float bars    = transport->position();
		float clamped = std::clamp(bars, 0.0f, (float)numCols);
		if (clamped != bars)
			transport->seek(clamped);
		if (obsTl && patternTrack < 0 && !loopActive)
			obsTl->syncActivePatterns(bars);
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

void Playhead::setLoopActive(bool a, std::function<bool(int)> enabledFn)
{
	loopActive     = a;
	loopEnabledFn  = std::move(enabledFn);
	if (owner) owner->redraw();
}

void Playhead::tick()
{
	if (patternTrack < 0 && transport) {
		if (transport->isPlaying()) {
			float curPos = transport->position();
			if (loopActive) {
				if (verbose && obsTl && curPos >= lastPosition)
					checkLoopVerboseNotes(lastPosition, curPos);
			} else {
				if (obsTl) obsTl->syncActivePatterns(curPos);
				if (verbose && obsTl && curPos >= lastPosition)
					checkVerboseNotes(lastPosition, curPos);
				if (curPos >= (float)numCols) {
					transport->pause();
					if (onEndReached) onEndReached();
				}
			}
			lastPosition = curPos;
		} else {
			lastPosition = transport->position();
		}
	} else if (transport) {
		lastPosition = transport->position();
	}
	if (owner && owner->visible_r()) owner->redraw();
}

void Playhead::checkVerboseNotes(float prevPos, float curPos)
{
	const Timeline& tl      = obsTl->get();
	const auto&     actives = obsTl->activePatterns();

	for (const auto& [patId, anchorBar] : actives) {
		const Pattern* pat = nullptr;
		std::string    label;
		for (const auto& p : tl.patterns)
			if (p.id == patId) { pat = &p; break; }
		if (!pat || pat->notes.empty() || pat->lengthBeats <= 0.0f) continue;
		for (const auto& track : tl.tracks)
			if (track.patternId == patId) { label = track.label; break; }

		int top, bottom;
		obsTl->timeSigAt((int)std::max(0.0f, anchorBar), top, bottom);
		float beatsPerBar = (float)top;
		float len         = pat->lengthBeats;
		float prevBeats   = (prevPos - anchorBar) * beatsPerBar;
		float curBeats    = (curPos  - anchorBar) * beatsPerBar;

		for (const Note& note : pat->notes) {
			if (note.disabled) continue;
			float cycles    = std::floor((prevBeats - note.beat) / len);
			float firstFire = note.beat + cycles * len;
			if (firstFire < prevBeats) firstFire += len;
			if (firstFire < curBeats) {
				float songBar = anchorBar + firstFire / beatsPerBar;
				int   bar     = (int)songBar + 1;
				int   beat    = (int)((songBar - std::floor(songBar)) * beatsPerBar) + 1;
				std::string name = pitchName ? pitchName(note.pitch)
				                             : std::to_string(note.pitch);
				printf("[verbose] bar %d beat %d | track \"%s\"  note=%-4s  beat=%.2f  len=%.2f\n",
				       bar, beat, label.c_str(), name.c_str(), note.beat, note.length);
			}
		}
	}
}

bool Playhead::isInPattern(float /*bars*/) const
{
	// Always show the playhead in the pattern editor — inactive patterns show
	// a virtual position indicating next-bar-aligned start (see barsToPixel).
	return true;
}

int Playhead::barsToPixel(float bars) const
{
	if (patternTrack >= 0) {
		if (!obsTl) return 0;
		const auto& tracks = obsTl->get().tracks;
		if (patternTrack >= (int)tracks.size()) return 0;
		int patId = tracks[patternTrack].patternId;

		// Time sig: use bar 0 in loop mode (loop editor's sig), current bar in song mode.
		int top, bottom;
		obsTl->timeSigAt(loopActive ? 0 : (int)std::max(0.0f, bars), top, bottom);

		if (obsTl->isPatternActive(patId)) {
			float anchor  = obsTl->patternAnchorBar(patId);
			float elapsed = (bars - anchor) * (float)top;
			float beats   = std::fmod(elapsed, (float)numCols);
			if (beats < 0.0f) beats += numCols;
			return std::clamp((int)(beats * colWidth), 0, numCols * colWidth - 2);
		}

		// Virtual position: beat 0 of the pattern will land on the next bar boundary.
		float nextBar     = std::floor(bars) + 1.0f;
		float beatsToNext = (nextBar - bars) * (float)top;
		float virtualBeat = std::fmod((float)numCols - beatsToNext, (float)numCols);
		if (virtualBeat < 0.0f) virtualBeat += numCols;
		return std::clamp((int)(virtualBeat * colWidth), 0, numCols * colWidth - 2);
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
	if (loopActive && patternTrack < 0) return;
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
	if (loopActive && patternTrack < 0) return;
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

void Playhead::checkLoopVerboseNotes(float prevPos, float curPos)
{
	if (!obsTl) return;
	const Timeline& tl      = obsTl->get();
	const auto&     actives = obsTl->activePatterns();

	for (const auto& [patId, anchorBar] : actives) {
		const Pattern* pat = nullptr;
		std::string    label;
		for (const auto& p : tl.patterns)
			if (p.id == patId) { pat = &p; break; }
		if (!pat || pat->lengthBeats <= 0.0f) continue;
		for (const auto& track : tl.tracks)
			if (track.patternId == patId) { label = track.label; break; }

		int top, bottom;
		obsTl->timeSigAt((int)std::max(0.0f, anchorBar), top, bottom);
		float beatsPerBar = (float)top;
		float len         = pat->lengthBeats;
		float prevBeats   = (prevPos - anchorBar) * beatsPerBar;
		float curBeats    = (curPos  - anchorBar) * beatsPerBar;

		for (const Note& note : pat->notes) {
			if (note.disabled) continue;
			float cycles    = std::floor((prevBeats - note.beat) / len);
			float firstFire = note.beat + cycles * len;
			if (firstFire < prevBeats) firstFire += len;
			if (firstFire < curBeats) {
				float songBar = anchorBar + firstFire / beatsPerBar;
				int   bar     = (int)songBar + 1;
				int   beat    = (int)(std::fmod(firstFire, beatsPerBar)) + 1;
				std::string name = pitchName ? pitchName(note.pitch) : std::to_string(note.pitch);
				printf("[verbose] bar %d beat %d | track \"%s\"  note=%-4s  beat=%.2f  len=%.2f\n",
				       bar, beat, label.c_str(), name.c_str(), note.beat, note.length);
			}
		}
	}
}

void Playhead::seek(int mouseX, int rulerX)
{
	if (!transport) return;
	float bars = pixelToBars(mouseX - rulerX);
	transport->seek(std::clamp(bars, 0.0f, (float)numCols));
}
