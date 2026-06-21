#include "playhead.hpp"
#include "port.hpp"
#include "portRegistry.hpp"
#include "chords.hpp"
#include "loopFiring.hpp"
#include "paramLaneTypes.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <cstdio>

static constexpr Fl_Color headColor     = 0xEF444400;  // red
static constexpr Fl_Color headColorDim  = 0xD1D5DB00;  // light grey

// Instrument of the track that holds an instance of `patId`. Used as the routing
// fallback for patterns with no instrument of their own (instrumentId 0).
static int trackInstrumentForPattern(const Timeline& tl, int patId)
{
	for (const auto& track : tl.tracks)
		for (const auto& lane : track.lanes)
			for (const auto& inst : lane.patterns)
				if (inst.patternId == patId)
					return track.instrumentId;
	return 0;
}

Playhead::Playhead(int numCols, int colWidth)
	: numCols(numCols), colWidth(colWidth)
{}

Playhead::~Playhead()
{
	Fl::remove_timeout(timerCb, this);
	swapObserver(obsTl, nullptr, this);
}

void Playhead::setTransport(ITransport* t, ObservableSong* tl)
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
		if (patternTrack < 0 && clamped != bars)
			transport->seek(clamped);
		if (aps && obsTl && patternTrack < 0 && !loopActive)
			aps->sync(*obsTl, bars);
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
		const bool playing   = transport->isPlaying();
		const bool runChecks = verbose || anySoftPort || (!jackClock && anyJackPort);
		if (playing) {
			float curPos = transport->position();
			if (loopActive) {
				if (curPos >= lastPosition) {
					if (runChecks && obsTl) checkLoopVerboseNotes(lastPosition, curPos);
				} else {
					allSoftNotesOff();   // loop wrapped — silence anything still on
				}
			} else {
				if (aps && obsTl) aps->sync(*obsTl, curPos);
				if (curPos >= lastPosition) {
					if (runChecks && obsTl) {
						checkVerboseNotes(lastPosition, curPos);
						checkVerboseSongParams(lastPosition, curPos);
					}
				} else {
					allSoftNotesOff();   // seeked backward
				}
				if (curPos >= (float)numCols) {
					transport->pause();
					if (onEndReached) onEndReached();
				}
			}
			flushSoftNoteOffs(curPos);
			lastPosition = curPos;
		} else {
			if (wasPlaying) allSoftNotesOff();   // stopped — release held notes
			float curPos = transport->position();
			if (aps && obsTl && !loopActive) aps->sync(*obsTl, curPos);
			lastPosition = curPos;
		}
		wasPlaying = playing;
	} else if (transport) {
		lastPosition = transport->position();
	}
	if (owner && owner->visible_r()) owner->redraw();
	if (onTick) onTick();
}

void Playhead::checkVerboseNotes(float prevPos, float curPos)
{
	const Timeline& tl      = obsTl->get();
	const auto&     actives = aps->patterns();

	for (const auto& [patId, anchorBar] : actives) {
		const Pattern* pat = nullptr;
		std::string    label;
		int            instrumentId = 0;
		for (const auto& p : tl.patterns)
			if (p.id == patId) { pat = &p; break; }
		if (!pat || pat->lengthBeats <= 0.0f) continue;
		if (pat->notes.empty() && pat->drumNotes.empty() && pat->paramLanes.empty()) continue;
		// Route by the pattern's own instrument (matches Sequencer). A lane can hold
		// instances of several patterns, so matching lanes[0].patternId would miss
		// every placed pattern except the one shown in the editor. Fall back to the
		// owning track's instrument when the pattern has none assigned (id 0).
		instrumentId = pat->instrumentId;
		if (instrumentId == 0)
			instrumentId = trackInstrumentForPattern(tl, patId);
		label = tl.instrumentName(instrumentId);

		int top, bottom;
		obsTl->timeSigAt((int)std::max(0.0f, anchorBar), top, bottom);
		float beatsPerBar = (float)top;
		float len         = pat->lengthBeats;
		float prevBeats   = (prevPos - anchorBar) * beatsPerBar;
		float curBeats    = (curPos  - anchorBar) * beatsPerBar;

		for (const Note& note : pat->notes) {
			if (note.disabled) continue;
			forEachFiring(note.beat, len, prevBeats, curBeats, [&](float firstFire) {
				float songBar = anchorBar + firstFire / beatsPerBar;
				int   bar     = (int)songBar + 1;
				int   beat    = (int)((songBar - std::floor(songBar)) * beatsPerBar) + 1;
				if (verbose) {
					std::string name = pitchName ? pitchName(note.row)
					                             : std::to_string(note.row);
					printf("[verbose] bar %d beat %d | track \"%s\"  note=%-4s  beat=%.2f  len=%.2f\n",
					       bar, beat, label.c_str(), name.c_str(), note.beat, note.length);
				}
				if (rowToMidi)
					emitSoftNoteOn(instrumentId, rowToMidi(note.row), note.velocity,
					               note.length, beatsPerBar, songBar);
			});
		}

		{
			bool anySolo = !pat->drumSolo.empty();
			for (const DrumNote& dn : pat->drumNotes) {
				bool isSolo = pat->drumSolo.count(dn.note) > 0;
				bool isMute = pat->drumMute.count(dn.note) > 0;
				if (isMute || (anySolo && !isSolo)) continue;
				forEachFiring(dn.beat, len, prevBeats, curBeats, [&](float firstFire) {
					float songBar = anchorBar + firstFire / beatsPerBar;
					int   bar     = (int)songBar + 1;
					int   beat    = (int)((songBar - std::floor(songBar)) * beatsPerBar) + 1;
					if (verbose)
						printf("[verbose] bar %d beat %d | track \"%s\"  drum=%d  beat=%.2f\n",
						       bar, beat, label.c_str(), dn.note, dn.beat);
					emitSoftNoteOn(instrumentId, dn.note, dn.velocity,
					               drumNoteLen, beatsPerBar, songBar);
				});
			}
		}

		for (const auto& lane : pat->paramLanes) {
			auto checkFire = [&](float evtBeat, int value) {
				forEachFiring(evtBeat, len, prevBeats, curBeats, [&](float firstFire) {
					float songBar = anchorBar + firstFire / beatsPerBar;
					int   bar     = (int)songBar + 1;
					int   beat    = (int)((songBar - std::floor(songBar)) * beatsPerBar) + 1;
					if (verbose)
						printf("[verbose] bar %d beat %d | track \"%s\"  param=%-12s  value=%d\n",
						       bar, beat, label.c_str(), lane.type.c_str(), value);
					emitSoftParam(instrumentId, ccForType(lane.type), value);
				});
			};
			for (int i = 0; i < (int)lane.points.size(); i++) {
				checkFire(lane.points[i].beat, lane.points[i].value);
				if (i + 1 < (int)lane.points.size())
					densifyParamRamp(lane.points[i].beat,  lane.points[i+1].beat,
					                 lane.points[i].value, lane.points[i+1].value, checkFire);
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
		int patId = tracks[patternTrack].lanes.empty() ? 0 : tracks[patternTrack].lanes[0].patternId;

		// Time sig: use bar 0 in loop mode (loop editor's sig), current bar in song mode.
		int top, bottom;
		obsTl->timeSigAt(loopActive ? 0 : (int)std::max(0.0f, bars), top, bottom);

		if (aps && aps->isPatternActive(patId)) {
			float anchor  = aps->patternAnchorBar(patId);
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

Fl_Color Playhead::currentHeadColor() const
{
	if (patternTrack >= 0 && obsTl && aps) {
		const auto& tracks = obsTl->get().tracks;
		if (patternTrack < (int)tracks.size()) {
			int patId = tracks[patternTrack].lanes.empty() ? 0 : tracks[patternTrack].lanes[0].patternId;
			if (!aps->isPatternActive(patId))
				return headColorDim;
		}
	}
	return headColor;
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
	fl_color(currentHeadColor());
	fl_polygon(px - tw/2, top, px + tw/2, top, px, tip);
}

void Playhead::drawLine(int gridX, int gridY, int gridH)
{
	if (!transport) return;
	if (loopActive && patternTrack < 0) return;
	float bars = transport->position();
	if (!isInPattern(bars)) return;
	int px = gridX + barsToPixel(bars);
	fl_color(currentHeadColor());
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
	const auto&     actives = aps->patterns();

	for (const auto& [patId, anchorBar] : actives) {
		const Pattern* pat = nullptr;
		std::string    label;
		int            instrumentId = 0;
		for (const auto& p : tl.patterns)
			if (p.id == patId) { pat = &p; break; }
		if (!pat || pat->lengthBeats <= 0.0f) continue;
		// Route by the pattern's own instrument (matches Sequencer); see checkVerboseNotes.
		instrumentId = pat->instrumentId;
		if (instrumentId == 0)
			instrumentId = trackInstrumentForPattern(tl, patId);
		label = tl.instrumentName(instrumentId);

		int top, bottom;
		obsTl->timeSigAt((int)std::max(0.0f, anchorBar), top, bottom);
		float beatsPerBar = (float)top;
		float len         = pat->lengthBeats;
		float prevBeats   = (prevPos - anchorBar) * beatsPerBar;
		float curBeats    = (curPos  - anchorBar) * beatsPerBar;

		for (const Note& note : pat->notes) {
			if (note.disabled) continue;
			forEachFiring(note.beat, len, prevBeats, curBeats, [&](float firstFire) {
				float songBar = anchorBar + firstFire / beatsPerBar;
				int   bar     = (int)songBar + 1;
				int   beat    = (int)(std::fmod(firstFire, beatsPerBar)) + 1;
				if (verbose) {
					std::string name = pitchName ? pitchName(note.row) : std::to_string(note.row);
					printf("[verbose] bar %d beat %d | track \"%s\"  note=%-4s  beat=%.2f  len=%.2f\n",
					       bar, beat, label.c_str(), name.c_str(), note.beat, note.length);
				}
				if (rowToMidi)
					emitSoftNoteOn(instrumentId, rowToMidi(note.row), note.velocity,
					               note.length, beatsPerBar, songBar);
			});
		}

		{
			bool anySolo = !pat->drumSolo.empty();
			for (const DrumNote& dn : pat->drumNotes) {
				bool isSolo = pat->drumSolo.count(dn.note) > 0;
				bool isMute = pat->drumMute.count(dn.note) > 0;
				if (isMute || (anySolo && !isSolo)) continue;
				forEachFiring(dn.beat, len, prevBeats, curBeats, [&](float firstFire) {
					float songBar = anchorBar + firstFire / beatsPerBar;
					int   bar     = (int)songBar + 1;
					int   beat    = (int)(std::fmod(firstFire, beatsPerBar)) + 1;
					if (verbose)
						printf("[verbose] bar %d beat %d | track \"%s\"  drum=%d  beat=%.2f\n",
						       bar, beat, label.c_str(), dn.note, dn.beat);
					emitSoftNoteOn(instrumentId, dn.note, dn.velocity,
					               drumNoteLen, beatsPerBar, songBar);
				});
			}
		}

		for (const auto& lane : pat->paramLanes) {
			auto checkFire = [&](float evtBeat, int value) {
				forEachFiring(evtBeat, len, prevBeats, curBeats, [&](float firstFire) {
					float songBar = anchorBar + firstFire / beatsPerBar;
					int   bar     = (int)songBar + 1;
					int   beat    = (int)(std::fmod(firstFire, beatsPerBar)) + 1;
					if (verbose)
						printf("[verbose] bar %d beat %d | track \"%s\"  param=%-12s  value=%d\n",
						       bar, beat, label.c_str(), lane.type.c_str(), value);
					emitSoftParam(instrumentId, ccForType(lane.type), value);
				});
			};
			for (int i = 0; i < (int)lane.points.size(); i++) {
				checkFire(lane.points[i].beat, lane.points[i].value);
				if (i + 1 < (int)lane.points.size())
					densifyParamRamp(lane.points[i].beat,  lane.points[i+1].beat,
					                 lane.points[i].value, lane.points[i+1].value, checkFire);
			}
		}
	}
}

void Playhead::checkVerboseSongParams(float prevPos, float curPos)
{
	if (!obsTl) return;
	const Timeline& tl = obsTl->get();

	// Each song-level param lane routes only to its owning instrument's port —
	// emitSoftParam filters to ports that need soft sequencing, so Jack-clock-driven
	// ports are left to JackTransport.
	for (const auto& lane : tl.paramLanes) {
		int cc = ccForType(lane.type);
		for (int i = 0; i < (int)lane.points.size(); i++) {
			auto report = [&](float barPos, int value) {
				if (barPos < prevPos || barPos >= curPos) return;
				if (verbose) {
					int bar = (int)barPos + 1;
					printf("[verbose] bar %d | song  param=%-12s  value=%d\n",
					       bar, lane.type.c_str(), value);
				}
				emitSoftParam(lane.instrumentId, cc, value);
			};
			report(lane.points[i].beat, lane.points[i].value);
			if (i + 1 < (int)lane.points.size()) {
				float b0 = lane.points[i].beat,  b1 = lane.points[i+1].beat;
				int   v0 = lane.points[i].value, v1 = lane.points[i+1].value;
				if (b1 > b0 && v1 != v0) {
					float db = b1 - b0;
					int   dv = v1 - v0;
					if (dv > 0)
						for (int N = v0; N < v1; N++)
							report(b0 + (N + 0.5f - v0) / dv * db, N + 1);
					else
						for (int N = v1; N < v0; N++)
							report(b0 + (N + 0.5f - v0) / dv * db, N);
				}
			}
		}
	}
}

// ── Soft (Native/Debug, plus Jack when it isn't the clock) output ─────────────

bool Playhead::portNeedsSoftSeq(const Port* p) const
{
	return p->softSequenced() || (!jackClock && p->backend() == MidiBackend::Jack);
}

void Playhead::emitSoftNoteOn(int instrumentId, int midi, float velocity,
                              float lenBeats, float beatsPerBar, float onBar)
{
	if (!portReg || !instrRoute) return;
	MidiInstrRoute r = instrRoute(instrumentId);
	if (r.portName.empty()) return;
	Port* p = portReg->find(r.portName);
	if (!p || !portNeedsSoftSeq(p)) return;
	int vel = (int)(velocity * 127.0f);
	vel = std::clamp(vel, 1, 127);
	p->noteOn(r.channel0, midi, vel);
	float offBar = onBar + (beatsPerBar > 0.0f ? lenBeats / beatsPerBar : 0.0f);
	softNotes.push_back({r.portName, r.channel0, midi, offBar});
}

void Playhead::emitSoftParam(int instrumentId, int ccNumber, int value)
{
	if (!portReg || !instrRoute) return;
	MidiInstrRoute r = instrRoute(instrumentId);
	if (r.portName.empty()) return;
	Port* p = portReg->find(r.portName);
	if (!p || !portNeedsSoftSeq(p)) return;
	if (ccNumber < 0) p->pitchBend(r.channel0, value);
	else              p->cc(r.channel0, ccNumber, value);
}

void Playhead::flushSoftNoteOffs(float curPos)
{
	for (auto it = softNotes.begin(); it != softNotes.end(); ) {
		if (it->offBar <= curPos) {
			if (Port* p = portReg ? portReg->find(it->portName) : nullptr)
				p->noteOff(it->channel, it->pitch);
			it = softNotes.erase(it);
		} else {
			++it;
		}
	}
}

void Playhead::allSoftNotesOff()
{
	if (portReg)
		for (const auto& n : softNotes)
			if (Port* p = portReg->find(n.portName)) p->noteOff(n.channel, n.pitch);
	softNotes.clear();
}

void Playhead::seek(int mouseX, int rulerX)
{
	if (!transport) return;
	float bars = pixelToBars(mouseX - rulerX);
	transport->seek(std::clamp(bars, 0.0f, (float)numCols));
}
