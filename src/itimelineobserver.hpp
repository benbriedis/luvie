#pragma once
#include <type_traits>

class ITimelineObserver {
public:
    virtual ~ITimelineObserver() = default;
    virtual void onTimelineChanged() = 0;
};

// std::type_identity_t on the second param prevents deduction from nullptr
template<typename T>
inline void swapObserver(T*& stored, std::type_identity_t<T*> next, ITimelineObserver* self)
{
    if (stored) stored->removeObserver(self);
    stored = next;
    if (stored) stored->addObserver(self);
}
