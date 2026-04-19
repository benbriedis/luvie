#ifndef ACTIVE_PATTERN_SET_HPP
#define ACTIVE_PATTERN_SET_HPP

#include "iActivePatternObserver.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ObservableTimeline;

// Runtime state tracking which patterns are currently playing and their phase.
// Populated by sync() in song mode; managed explicitly via activate/deactivate
// in loop mode. Separate from ObservableTimeline (persistent song data).
class ActivePatternSet {
public:
	// Song-mode sync: compute what should be active from the timeline at currentBar
	// and reconcile with any manual overrides. Notifies observers if anything changed.
	void sync(const ObservableTimeline& tl, float currentBar);

	// Loop-mode explicit control. Both notify observers.
	void activate(int patId, float anchorBar);
	void deactivate(int patId);

	// Clear all state (called on mode switch). Notifies if non-empty.
	void clear();

	bool  isPatternActive(int patId) const;
	float patternAnchorBar(int patId) const;  // 0.0f if not active
	const std::unordered_map<int, float>& patterns() const { return activePats; }

	void addObserver(IActivePatternObserver* o);
	void removeObserver(IActivePatternObserver* o);

private:
	std::unordered_map<int, float> activePats;
	std::unordered_set<int>        manualActive;
	std::unordered_set<int>        manuallyDisabled;
	std::unordered_set<int>        songOriginated;
	std::vector<IActivePatternObserver*> observers;

	void notify();
};

inline void swapActiveObserver(ActivePatternSet*& stored, ActivePatternSet* next,
                               IActivePatternObserver* self)
{
	if (stored) stored->removeObserver(self);
	stored = next;
	if (stored) stored->addObserver(self);
}

#endif
