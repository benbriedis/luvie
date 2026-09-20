// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SCENE_BANK_HPP
#define SCENE_BANK_HPP

#include <array>
#include <set>
#include <unordered_map>
#include <vector>

// The Loop Editor's scenes: five boards over the one grid of pattern blocks, alike in
// every way but which blocks are switched on.
//
// Slot 0 is "Scene S", the song-linked scene, and it is a *mirror* rather than a set
// this class authors: it is refreshed from LoopManager while Scene S is the shown
// scene and frozen as last seen once another scene is shown. That is what lets the
// grid draw one scene's blocks while a different one is still sounding — the armed
// window between a click and the bar line it lands on. Its content is already
// persisted as AppState::activeLoopPatterns, so it is never saved from here.
//
// Slots 1..4 are the user's own. The song never writes them; only a click does.
//
// This is a store of sets, not a playback authority — LoopManager remains the single
// authority for what is actually sounding and at what phase. Runtime state, persisted
// through AppState rather than the Timeline: a scene switch is a performance action,
// not a song edit, so it has no business in undo/redo.
class SceneBank {
public:
	static constexpr int kSceneSong = 0;   // "Scene S"
	static constexpr int kScenes    = 5;   // slot 0 plus Scenes 1..4
	static constexpr int kUserScenes = kScenes - 1;

	static bool isUserScene(int scene) { return scene >= 1 && scene < kScenes; }

	// What the grid draws and what a click edits.
	int  shownScene()   const { return shown; }
	// What LoopManager has actually been loaded with. Differs from shownScene() only
	// while a switch is armed and waiting for its bar line.
	int  playingScene() const { return playing; }
	void setShown(int s);
	void setPlaying(int s)    { playing = clamp(s); }
	// True while a scene switch is armed: the grid is showing one scene and another
	// is still sounding. The scene button draws amber for exactly this.
	bool switchPending() const { return shown != playing; }

	bool isEnabled(int patId) const { return isEnabled(shown, patId); }
	bool isEnabled(int scene, int patId) const;
	void toggle(int scene, int patId);
	void setEnabled(int scene, int patId, bool on);

	const std::set<int>& set(int scene) const { return sets[clamp(scene)]; }

	// Adopt LoopManager's live set as Scene S's. Called on every loop change while
	// Scene S is shown; a no-op otherwise, which is what freezes the mirror.
	void mirrorSceneS(const std::unordered_map<int, float>& live);

	// Drop pattern ids that no longer exist — a pattern deleted in the Song Editor, or
	// a project loaded whose scenes outlived their patterns. Scene S is left alone: it
	// is a mirror and LoopManager has already reconciled itself.
	void prunePatterns(const std::set<int>& existingIds);

	// Persistence. Only the user scenes travel; see the note above about slot 0.
	std::array<std::vector<int>, kUserScenes> save() const;
	void load(const std::array<std::vector<int>, kUserScenes>& saved, int shownScene);

private:
	static int clamp(int s) { return (s < 0 || s >= kScenes) ? kSceneSong : s; }

	std::array<std::set<int>, kScenes> sets;   // ordered: save() wants them ascending
	int shown   = kSceneSong;
	int playing = kSceneSong;
};

#endif
