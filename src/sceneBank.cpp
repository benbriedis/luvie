// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "sceneBank.hpp"

void SceneBank::setShown(int s)
{
	shown = clamp(s);
}

bool SceneBank::isEnabled(int scene, int patId) const
{
	return sets[clamp(scene)].count(patId) > 0;
}

void SceneBank::toggle(int scene, int patId)
{
	setEnabled(scene, patId, !isEnabled(scene, patId));
}

void SceneBank::setEnabled(int scene, int patId, bool on)
{
	auto& s = sets[clamp(scene)];
	if (on) s.insert(patId);
	else    s.erase(patId);
}

void SceneBank::mirrorSceneS(const std::unordered_map<int, float>& live)
{
	// Only while Scene S is the one on screen. Once another scene is shown the mirror
	// holds what Scene S last looked like, so switching back to it shows that rather
	// than whatever the song has done in the meantime.
	if (shown != kSceneSong) return;
	auto& s = sets[kSceneSong];
	s.clear();
	for (const auto& [patId, anchor] : live) s.insert(patId);
}

void SceneBank::prunePatterns(const std::set<int>& existingIds)
{
	for (int i = 1; i < kScenes; i++) {
		auto& s = sets[i];
		for (auto it = s.begin(); it != s.end(); )
			it = existingIds.count(*it) ? std::next(it) : s.erase(it);
	}
}

std::array<std::vector<int>, SceneBank::kUserScenes> SceneBank::save() const
{
	std::array<std::vector<int>, kUserScenes> out;
	for (int i = 0; i < kUserScenes; i++)
		out[i].assign(sets[i + 1].begin(), sets[i + 1].end());   // std::set: ascending
	return out;
}

void SceneBank::load(const std::array<std::vector<int>, kUserScenes>& saved, int shownScene)
{
	for (int i = 0; i < kUserScenes; i++)
		sets[i + 1] = std::set<int>(saved[i].begin(), saved[i].end());
	sets[kSceneSong].clear();   // a mirror: LoopManager refills it on the next change
	shown = playing = clamp(shownScene);
}
