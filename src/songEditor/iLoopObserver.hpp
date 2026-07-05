#ifndef ILOOP_OBSERVER_HPP
#define ILOOP_OBSERVER_HPP

class ILoopObserver {
public:
	virtual void onLoopsChanged() = 0;
	virtual ~ILoopObserver() = default;
};

#endif
