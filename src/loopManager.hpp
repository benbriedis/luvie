#ifndef LOOP_MANAGER_HPP
#define LOOP_MANAGER_HPP

#include "iLoopObserver.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ObservableSong;

// The single authority for which patterns are currently playing and their phase.
// Two masters feed it: the timeline (via sync(), reflecting song pattern blocks)
// and the Loop Editor (via activate()/deactivate(), the manual switches). It
// reconciles them — manual overrides win over the song — into one deduped active
// set that both consumers read: the DSP Sequencer and the Loop Editor display.
// Runtime state only; separate from ObservableSong (persistent song data).
class LoopManager {
public:
	// Song-mode sync: compute what should be active from the timeline at currentBar
	// and reconcile with any manual overrides. Notifies observers if anything changed.
	void sync(const ObservableSong& tl, float currentBar);

	// Loop-mode explicit control. Both notify observers.
	void activate(int patId, float anchorBar);
	void deactivate(int patId);

	// Clear all state (called on mode switch). Notifies if non-empty.
	void clear();

	bool  isPatternActive(int patId) const;
	float patternAnchorBar(int patId) const;  // 0.0f if not active
	// True for patterns turned on by a Loop-Editor switch (as opposed to a song
	// pattern block). Lets consumers layer manual loops over song playback.
	bool  isManual(int patId) const { return manualActive.count(patId) > 0; }
	// True while a Loop-Editor switch has silenced a pattern for its current song
	// placement. The DSP defers to this so a manual disable stops the sound, not
	// just the UI. Cleared by sync() when the next placement of the pattern starts.
	bool  isManuallyDisabled(int patId) const { return manuallyDisabled.count(patId) > 0; }
	const std::unordered_map<int, float>& patterns() const { return activePats; }

	void addObserver(ILoopObserver* o);
	void removeObserver(ILoopObserver* o);

private:
	std::unordered_map<int, float> activePats;
	std::unordered_set<int>        manualActive;
	std::unordered_set<int>        manuallyDisabled;
	std::unordered_set<int>        songOriginated;
	std::vector<ILoopObserver*> observers;

	void notify();
};

inline void swapLoopObserver(LoopManager*& stored, LoopManager* next,
                               ILoopObserver* self)
{
	if (stored) stored->removeObserver(self);
	stored = next;
	if (stored) stored->addObserver(self);
}

#endif
