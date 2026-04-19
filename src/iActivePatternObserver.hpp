#ifndef IACTIVE_PATTERN_OBSERVER_HPP
#define IACTIVE_PATTERN_OBSERVER_HPP

class IActivePatternObserver {
public:
	virtual void onActivePatternsChanged() = 0;
	virtual ~IActivePatternObserver() = default;
};

#endif
