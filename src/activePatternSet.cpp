#include "activePatternSet.hpp"
#include "observableTimeline.hpp"
#include "timeline.hpp"
#include <algorithm>

void ActivePatternSet::notify()
{
	for (auto* o : observers) o->onActivePatternsChanged();
}

void ActivePatternSet::addObserver(IActivePatternObserver* o)
{
	observers.push_back(o);
}

void ActivePatternSet::removeObserver(IActivePatternObserver* o)
{
	observers.erase(std::remove(observers.begin(), observers.end(), o), observers.end());
}

void ActivePatternSet::sync(const ObservableTimeline& tl, float currentBar)
{
	const Timeline& data = tl.get();

	std::unordered_map<int, float> songResult;
	for (const auto& track : data.tracks) {
		for (const auto& inst : track.patterns) {
			if (currentBar < inst.startBar || currentBar >= inst.startBar + inst.length)
				continue;
			const Pattern* pat = nullptr;
			for (const auto& p : data.patterns)
				if (p.id == inst.patternId) { pat = &p; break; }
			if (!pat || pat->lengthBeats <= 0.0f) break;
			int top, bottom;
			tl.timeSigAt((int)inst.startBar, top, bottom);
			songResult[inst.patternId] = inst.startBar - inst.startOffset / (float)top;
			break;  // at most one active instance per track
		}
	}

	bool changed = false;

	// Activate song-wanted patterns.
	// Only on instance START (patId newly entering songResult) do we clear
	// manuallyDisabled, so the song regains authority at each new instance.
	// Within a running instance, manual overrides (enable/disable) are respected.
	for (const auto& [patId, anchor] : songResult) {
		bool isNewInstance = !songOriginated.count(patId);
		if (isNewInstance)
			manuallyDisabled.erase(patId);
		if (manuallyDisabled.count(patId)) continue;
		if (manualActive.count(patId)) continue;  // keep manual anchorBar
		auto it = activePats.find(patId);
		if (it == activePats.end() || it->second != anchor) {
			activePats[patId] = anchor;
			changed = true;
		}
	}

	// Deactivate song-originated patterns that are no longer wanted. Ending an
	// instance clears all manual state so the next instance starts fresh.
	for (int patId : songOriginated) {
		if (songResult.count(patId)) continue;
		manualActive.erase(patId);
		manuallyDisabled.erase(patId);
		if (activePats.erase(patId)) changed = true;
	}

	songOriginated.clear();
	for (const auto& [patId, _] : songResult) songOriginated.insert(patId);

	if (changed) notify();
}

void ActivePatternSet::activate(int patId, float anchorBar)
{
	manuallyDisabled.erase(patId);
	manualActive.insert(patId);
	activePats[patId] = anchorBar;
	notify();
}

void ActivePatternSet::deactivate(int patId)
{
	manualActive.erase(patId);
	manuallyDisabled.insert(patId);
	if (activePats.erase(patId))
		notify();
}

void ActivePatternSet::clear()
{
	bool hadContent = !activePats.empty() || !manualActive.empty()
	               || !manuallyDisabled.empty();
	activePats.clear();
	manualActive.clear();
	manuallyDisabled.clear();
	songOriginated.clear();
	if (hadContent) notify();
}

bool ActivePatternSet::isPatternActive(int patId) const
{
	return activePats.count(patId) > 0;
}

float ActivePatternSet::patternAnchorBar(int patId) const
{
	auto it = activePats.find(patId);
	return it != activePats.end() ? it->second : 0.0f;
}
