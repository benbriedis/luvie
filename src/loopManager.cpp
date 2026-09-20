// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

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

void LoopManager::restore(const std::vector<int>& patterns, float anchorBar)
{
	std::unordered_map<int, float> next;
	for (int patId : patterns) next[patId] = anchorBar;

	bool changed = next != activePats || !manualActive.empty() || !manuallyDisabled.empty();
	activePats = std::move(next);
	manualActive.clear();
	manuallyDisabled.clear();
	// Left empty so the first sync() after a switch back to Song mode treats every
	// timeline placement as a new instance rather than a continuing one.
	songOriginated.clear();
	if (changed) notify();
}

void LoopManager::applySet(const std::set<int>& patterns, float anchorBarForNew)
{
	std::unordered_map<int, float> next;
	for (int patId : patterns) {
		// A pattern already sounding keeps its anchor, and so its phase: switching
		// scenes must not restart a loop that is in both of them. Only a newly
		// enabled one is phased, onto anchorBarForNew.
		auto it = activePats.find(patId);
		next[patId] = (it != activePats.end()) ? it->second : anchorBarForNew;
	}

	bool changed = next != activePats || !manualActive.empty() || !manuallyDisabled.empty();
	activePats = std::move(next);
	// A scene is a set, not a set of overrides. Clearing these is what stops scene
	// state outliving a switch back to Song mode, where the Sequencer reads both when
	// deciding whether to play a song block (see sequencer.cpp, buildSnapshot).
	manualActive.clear();
	manuallyDisabled.clear();
	songOriginated.clear();
	if (changed) notify();
}

void LoopManager::armScene(const std::set<int>& next, float atBar)
{
	pendingActives.clear();
	for (int patId : next) {
		auto it = activePats.find(patId);
		pendingActives[patId] = (it != activePats.end()) ? it->second : atBar;
	}
	pendingScene = true;
	pendingAtBar = atBar;
	notify();
}

void LoopManager::commitScene()
{
	if (!pendingScene) return;
	pendingScene = false;
	activePats = std::move(pendingActives);
	pendingActives.clear();
	// Same reasoning as applySet(): a scene is a set, not a set of overrides.
	manualActive.clear();
	manuallyDisabled.clear();
	songOriginated.clear();
	// Always notifies, even when the two scenes held the same patterns: observers
	// track the armed/landed distinction as well as the set, and a switch that
	// changed no pattern still has to clear the pending state they are showing.
	notify();
}

void LoopManager::cancelScene()
{
	if (!pendingScene) return;
	pendingScene = false;
	pendingActives.clear();
	notify();
}

void LoopManager::mirrorArmedScene(const std::unordered_map<int, float>& pending,
                                   float atBar)
{
	pendingActives = pending;
	pendingScene   = true;
	pendingAtBar   = atBar;
}

void LoopManager::reanchor(int patId, float anchorBar)
{
	auto it = activePats.find(patId);
	if (it == activePats.end() || it->second == anchorBar) return;
	it->second = anchorBar;
	notify();
}

void LoopManager::reanchorAll(float anchorBar)
{
	bool changed = false;
	for (auto& [patId, anchor] : activePats) {
		if (anchor != anchorBar) { anchor = anchorBar; changed = true; }
	}
	if (changed) notify();
}

void LoopManager::mirror(const std::unordered_map<int, float>& actives,
                         const std::unordered_set<int>& manual,
                         const std::unordered_set<int>& disabled)
{
	if (actives == activePats && manual == manualActive && disabled == manuallyDisabled
	    && !pendingScene)
		return;
	// An authoritative full replacement, so any armed scene goes with it: left set, a
	// stale pending map would later commit over the state just mirrored.
	pendingScene = false;
	pendingActives.clear();
	activePats      = actives;
	manualActive    = manual;
	manuallyDisabled = disabled;
	notify();
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
