#include "observableTimeline.hpp"
#include <algorithm>
#include <set>
#include <cmath>

ObservableTimeline::ObservableTimeline(float initBpm, int initTop, int initBottom)
{
	data.bpms.push_back({0, initBpm});
	data.timeSigs.push_back({0, initTop, initBottom});
}

void ObservableTimeline::addObserver(ITimelineObserver* o)
{
	observers.push_back(o);
}

void ObservableTimeline::removeObserver(ITimelineObserver* o)
{
	observers.erase(std::remove(observers.begin(), observers.end(), o), observers.end());
}

void ObservableTimeline::notify()
{
	for (auto* o : observers) o->onTimelineChanged();
}

void ObservableTimeline::sortBpms()
{
	std::sort(data.bpms.begin(), data.bpms.end(),
		[](const BpmMarker& a, const BpmMarker& b) { return a.bar < b.bar; });
}

void ObservableTimeline::sortTimeSigs()
{
	std::sort(data.timeSigs.begin(), data.timeSigs.end(),
		[](const TimeSigMarker& a, const TimeSigMarker& b) { return a.bar < b.bar; });
}

void ObservableTimeline::setBpm(int bar, float bpm)
{
	for (auto& m : data.bpms) {
		if (m.bar == bar) { m.bpm = bpm; notify(); return; }
	}
	data.bpms.push_back({bar, bpm});
	sortBpms();
	notify();
}

void ObservableTimeline::removeBpm(int bar)
{
	if (bar == 0) return;
	data.bpms.erase(std::remove_if(data.bpms.begin(), data.bpms.end(),
		[bar](const BpmMarker& m) { return m.bar == bar; }), data.bpms.end());
	notify();
}

void ObservableTimeline::moveBpmMarker(int fromBar, int toBar)
{
	if (fromBar == 0) return;
	auto it = std::find_if(data.bpms.begin(), data.bpms.end(),
		[fromBar](const BpmMarker& m) { return m.bar == fromBar; });
	if (it == data.bpms.end()) return;
	float val = it->bpm;
	data.bpms.erase(it);
	data.bpms.erase(std::remove_if(data.bpms.begin(), data.bpms.end(),
		[toBar](const BpmMarker& m) { return m.bar == toBar; }), data.bpms.end());
	data.bpms.push_back({toBar, val});
	sortBpms();
	notify();
}

float ObservableTimeline::bpmAt(int bar) const
{
	float result = 120.0f;
	for (auto& m : data.bpms) {
		if (m.bar <= bar) result = m.bpm;
		else break;
	}
	return result;
}

void ObservableTimeline::setTimeSig(int bar, int top, int bottom)
{
	for (auto& m : data.timeSigs) {
		if (m.bar == bar) { m.top = top; m.bottom = bottom; notify(); return; }
	}
	data.timeSigs.push_back({bar, top, bottom});
	sortTimeSigs();
	notify();
}

void ObservableTimeline::removeTimeSig(int bar)
{
	if (bar == 0) return;
	data.timeSigs.erase(std::remove_if(data.timeSigs.begin(), data.timeSigs.end(),
		[bar](const TimeSigMarker& m) { return m.bar == bar; }), data.timeSigs.end());
	notify();
}

void ObservableTimeline::moveTimeSigMarker(int fromBar, int toBar)
{
	if (fromBar == 0) return;
	auto it = std::find_if(data.timeSigs.begin(), data.timeSigs.end(),
		[fromBar](const TimeSigMarker& m) { return m.bar == fromBar; });
	if (it == data.timeSigs.end()) return;
	int top = it->top, bottom = it->bottom;
	data.timeSigs.erase(it);
	data.timeSigs.erase(std::remove_if(data.timeSigs.begin(), data.timeSigs.end(),
		[toBar](const TimeSigMarker& m) { return m.bar == toBar; }), data.timeSigs.end());
	data.timeSigs.push_back({toBar, top, bottom});
	sortTimeSigs();
	notify();
}

void ObservableTimeline::timeSigAt(int bar, int& top, int& bottom) const
{
	top = 4; bottom = 4;
	for (auto& m : data.timeSigs) {
		if (m.bar <= bar) { top = m.top; bottom = m.bottom; }
		else break;
	}
}

// ---------------------------------------------------------------------------

std::vector<ObservableTimeline::TimeSegment> ObservableTimeline::buildSegments() const
{
	std::set<int> breakpoints;
	for (auto& m : data.bpms)    breakpoints.insert(m.bar);
	for (auto& m : data.timeSigs) breakpoints.insert(m.bar);

	std::vector<TimeSegment> segs;
	for (int bar : breakpoints) {
		int top, bottom;
		timeSigAt(bar, top, bottom);
		segs.push_back({bar, bpmAt(bar), top});
	}
	return segs;
}

double ObservableTimeline::barToSeconds(float targetBar) const
{
	auto segs = buildSegments();
	double secs = 0.0;
	for (int i = 0; i < (int)segs.size(); i++) {
		float segStart = (float)segs[i].bar;
		float segEnd   = (i + 1 < (int)segs.size()) ? (float)segs[i+1].bar : targetBar;
		if (segStart >= targetBar) break;
		float barsInSeg = std::min(segEnd, targetBar) - segStart;
		double secsPerBar = segs[i].beatsPerBar * 60.0 / segs[i].bpm;
		secs += barsInSeg * secsPerBar;
	}
	return secs;
}

float ObservableTimeline::secondsToBar(double secs) const
{
	auto segs = buildSegments();
	double remaining = secs;
	for (int i = 0; i < (int)segs.size(); i++) {
		float  segStart   = (float)segs[i].bar;
		float  segEnd     = (i + 1 < (int)segs.size()) ? (float)segs[i+1].bar : 1e9f;
		double secsPerBar = segs[i].beatsPerBar * 60.0 / segs[i].bpm;
		double segSecs    = (segEnd - segStart) * secsPerBar;
		if (remaining < segSecs + 1e-9)
			return segStart + (float)(remaining / secsPerBar);
		remaining -= segSecs;
	}
	if (!segs.empty()) {
		auto& last = segs.back();
		double secsPerBar = last.beatsPerBar * 60.0 / last.bpm;
		return (float)(last.bar + remaining / secsPerBar);
	}
	return 0.0f;
}

void ObservableTimeline::secondsToBarBeat(double secs, int& bar, int& beat) const
{
	float barF   = secondsToBar(secs);
	int   barInt = (int)barF;
	int   top, bottom;
	timeSigAt(barInt, top, bottom);
	bar  = barInt + 1;
	beat = (int)((barF - (float)barInt) * top) + 1;
}

// ---------------------------------------------------------------------------
// Track management

int ObservableTimeline::addTrack(std::string label, int patternId)
{
	int id = nextId++;
	data.tracks.push_back({id, std::move(label), patternId, {}});
	notify();
	return id;
}

void ObservableTimeline::renameTrack(int trackId, std::string newLabel)
{
	for (auto& t : data.tracks)
		if (t.id == trackId) { t.label = std::move(newLabel); notify(); return; }
}

void ObservableTimeline::selectTrack(int index)
{
	data.selectedTrackIndex = index;
	notify();
}

void ObservableTimeline::removeTrack(int trackId)
{
	data.tracks.erase(
		std::remove_if(data.tracks.begin(), data.tracks.end(),
			[trackId](const Track& t) { return t.id == trackId; }),
		data.tracks.end());
	notify();
}

int ObservableTimeline::copyPattern(int srcPatId)
{
	const Pattern* src = nullptr;
	for (const auto& p : data.patterns)
		if (p.id == srcPatId) { src = &p; break; }
	if (!src) return -1;

	Pattern copy;
	copy.id = nextId++;
	copy.lengthBeats = src->lengthBeats;
	for (auto n : src->notes) {
		n.id = nextId++;
		copy.notes.push_back(n);
	}
	data.patterns.push_back(copy);
	return copy.id;
}

void ObservableTimeline::removeTrackAndPattern(int trackId)
{
	auto it = std::find_if(data.tracks.begin(), data.tracks.end(),
		[trackId](const Track& t) { return t.id == trackId; });
	if (it == data.tracks.end()) return;

	int patId = it->patternId;
	data.tracks.erase(it);

	bool stillUsed = std::any_of(data.tracks.begin(), data.tracks.end(),
		[patId](const Track& t) { return t.patternId == patId; });
	if (!stillUsed && patId > 0)
		data.patterns.erase(std::remove_if(data.patterns.begin(), data.patterns.end(),
			[patId](const Pattern& p) { return p.id == patId; }), data.patterns.end());

	if (data.selectedTrackIndex >= (int)data.tracks.size())
		data.selectedTrackIndex = std::max(0, (int)data.tracks.size() - 1);
	notify();
}

// ---------------------------------------------------------------------------
// Pattern definition management

int ObservableTimeline::createPattern(float lengthBeats)
{
	int id = nextId++;
	data.patterns.push_back({id, lengthBeats});
	notify();
	return id;
}

const PatternInstance* ObservableTimeline::instanceById(int instanceId) const
{
	for (const auto& track : data.tracks)
		for (const auto& inst : track.patterns)
			if (inst.id == instanceId)
				return &inst;
	return nullptr;
}

const Pattern* ObservableTimeline::patternForInstance(int instanceId) const
{
	for (const auto& track : data.tracks)
		for (const auto& inst : track.patterns)
			if (inst.id == instanceId) {
				for (const auto& pat : data.patterns)
					if (pat.id == inst.patternId)
						return &pat;
				return nullptr;
			}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Pattern instance management

void ObservableTimeline::addPattern(int trackIndex, float startBar, float length, float patternBeats)
{
	if (trackIndex < 0 || trackIndex >= (int)data.tracks.size()) return;
	int patId = 0;
	if (patternBeats > 0.0f) {
		patId = nextId++;
		data.patterns.push_back({patId, patternBeats});
	}
	data.tracks[trackIndex].patterns.push_back({nextId++, patId, startBar, length});
	notify();
}

void ObservableTimeline::removePattern(int instanceId)
{
	for (auto& track : data.tracks) {
		auto it = std::find_if(track.patterns.begin(), track.patterns.end(),
			[instanceId](const PatternInstance& p) { return p.id == instanceId; });
		if (it != track.patterns.end()) {
			int patId = it->patternId;
			track.patterns.erase(it);
			// Remove pattern definition if nothing else references it
			if (patId > 0) {
				bool stillUsed = false;
				for (const auto& t : data.tracks)
					for (const auto& p : t.patterns)
						if (p.patternId == patId) { stillUsed = true; break; }
				if (!stillUsed)
					data.patterns.erase(std::remove_if(data.patterns.begin(), data.patterns.end(),
						[patId](const Pattern& p) { return p.id == patId; }), data.patterns.end());
			}
			notify();
			return;
		}
	}
}

void ObservableTimeline::movePattern(int instanceId, int newTrackIndex, float newStartBar)
{
	if (newTrackIndex < 0 || newTrackIndex >= (int)data.tracks.size()) return;
	for (auto& track : data.tracks) {
		auto it = std::find_if(track.patterns.begin(), track.patterns.end(),
			[instanceId](const PatternInstance& p) { return p.id == instanceId; });
		if (it != track.patterns.end()) {
			PatternInstance inst = *it;
			track.patterns.erase(it);
			inst.startBar = newStartBar;
			data.tracks[newTrackIndex].patterns.push_back(std::move(inst));
			notify();
			return;
		}
	}
}

void ObservableTimeline::resizePattern(int patternId, float newLength)
{
	for (auto& track : data.tracks) {
		for (auto& p : track.patterns) {
			if (p.id == patternId) {
				p.length = newLength;
				notify();
				return;
			}
		}
	}
}

void ObservableTimeline::resizePatternLeft(int patternId, float newStartBar, float newLength, float newStartOffset)
{
	for (auto& track : data.tracks)
		for (auto& p : track.patterns)
			if (p.id == patternId) {
				p.startBar    = newStartBar;
				p.length      = newLength;
				p.startOffset = newStartOffset;
				notify();
				return;
			}
}

void ObservableTimeline::setPatternStartOffset(int patternId, float startOffset)
{
	for (auto& track : data.tracks)
		for (auto& p : track.patterns)
			if (p.id == patternId) { p.startOffset = startOffset; notify(); return; }
}

std::vector<Note> ObservableTimeline::buildPatternNotes(int patternId) const
{
	for (const auto& pat : data.patterns)
		if (pat.id == patternId)
			return pat.notes;
	return {};
}

void ObservableTimeline::addNote(int patternId, float start, float pitch, float length, float velocity)
{
	for (auto& pat : data.patterns) {
		if (pat.id == patternId) {
			pat.notes.push_back({nextId++, (int)pitch, start, length, velocity});
			notify();
			return;
		}
	}
}

void ObservableTimeline::removeNote(int noteId)
{
	for (auto& pat : data.patterns) {
		auto it = std::find_if(pat.notes.begin(), pat.notes.end(),
			[noteId](const Note& n) { return n.id == noteId; });
		if (it != pat.notes.end()) {
			pat.notes.erase(it);
			notify();
			return;
		}
	}
}

void ObservableTimeline::moveNote(int noteId, float newStart, float newPitch)
{
	for (auto& pat : data.patterns) {
		for (auto& n : pat.notes) {
			if (n.id == noteId) {
				n.beat = newStart;
				n.pitch = (int)newPitch;
				notify();
				return;
			}
		}
	}
}

void ObservableTimeline::resizeNoteRight(int noteId, float newLength)
{
	for (auto& pat : data.patterns) {
		for (auto& n : pat.notes) {
			if (n.id == noteId) {
				n.length = newLength;
				notify();
				return;
			}
		}
	}
}

void ObservableTimeline::resizeNoteLeft(int noteId, float newStart, float newLength)
{
	for (auto& pat : data.patterns) {
		for (auto& n : pat.notes) {
			if (n.id == noteId) {
				n.beat    = newStart;
				n.length = newLength;
				notify();
				return;
			}
		}
	}
}

void ObservableTimeline::placePattern(int trackIndex, int patternId, float startBar, float length)
{
	if (trackIndex < 0 || trackIndex >= (int)data.tracks.size()) return;
	data.tracks[trackIndex].patterns.push_back({nextId++, patternId, startBar, length});
	notify();
}

std::vector<Note> ObservableTimeline::buildNotes() const
{
	std::vector<Note> notes;
	for (int row = 0; row < (int)data.tracks.size(); row++)
		for (const auto& p : data.tracks[row].patterns)
			notes.push_back({p.id, row, p.startBar, p.length});
	return notes;
}
