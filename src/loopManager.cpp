#include "loopManager.hpp"
#include "observableSong.hpp"
#include "timeline.hpp"
#include <algorithm>

void LoopManager::notify()
{
	for (auto* o : observers) o->onLoopsChanged();
}

void LoopManager::addObserver(ILoopObserver* o)
{
	observers.push_back(o);
}

void LoopManager::removeObserver(ILoopObserver* o)
{
	observers.erase(std::remove(observers.begin(), observers.end(), o), observers.end());
}

void LoopManager::sync(const ObservableSong& tl, float currentBar)
{
	const Timeline& data = tl.get();

	bool anySolo = std::any_of(data.tracks.begin(), data.tracks.end(),
	                           [](const Track& t) { return t.solo; });

	std::unordered_map<int, float> songResult;
	for (const auto& track : data.tracks) {
		if (track.mute || (anySolo && !track.solo)) continue;
		for (const auto& lane : track.lanes) {
			for (const auto& inst : lane.patterns) {
				if (currentBar < inst.startBar || currentBar >= inst.startBar + inst.length)
					continue;
				const Pattern* pat = nullptr;
				for (const auto& p : data.patterns)
					if (p.id == inst.patternId) { pat = &p; break; }
				if (!pat || pat->lengthBeats <= 0.0f) break;
				float beatsPerBar = tl.patternBeatsPerBar((int)inst.startBar, *pat);
				songResult[inst.patternId] = inst.startBar - inst.startOffset / beatsPerBar;
				break;  // at most one active instance per lane
			}
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

void LoopManager::activate(int patId, float anchorBar)
{
	manuallyDisabled.erase(patId);
	manualActive.insert(patId);
	activePats[patId] = anchorBar;
	notify();
}

void LoopManager::deactivate(int patId)
{
	manualActive.erase(patId);
	manuallyDisabled.insert(patId);
	if (activePats.erase(patId))
		notify();
}

void LoopManager::clear()
{
	bool hadContent = !activePats.empty() || !manualActive.empty()
	               || !manuallyDisabled.empty();
	activePats.clear();
	manualActive.clear();
	manuallyDisabled.clear();
	songOriginated.clear();
	if (hadContent) notify();
}

bool LoopManager::isPatternActive(int patId) const
{
	return activePats.count(patId) > 0;
}

float LoopManager::patternAnchorBar(int patId) const
{
	auto it = activePats.find(patId);
	return it != activePats.end() ? it->second : 0.0f;
}
