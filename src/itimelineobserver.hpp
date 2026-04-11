#pragma once

class ITimelineObserver {
public:
    virtual ~ITimelineObserver() = default;
    virtual void onTimelineChanged() = 0;
};
