// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <type_traits>

class ITimelineObserver {
public:
    virtual ~ITimelineObserver() = default;
    virtual void onTimelineChanged() = 0;
};

// The global tempo register (ObservableSong's "Global tempo" block) gets a channel of
// its own, deliberately separate from ITimelineObserver. The register is not song
// content — it appears nowhere in the Timeline and nowhere in the saved project — so a
// change to it must not look like a project edit. Announced as one, it would serialize
// and re-send the entire song to the LV2 DSP, rebuild the engine snapshot, and push an
// undo mirror, all for a value none of that carries; a dragged BPM spinner does that
// dozens of times a second, and the result is audible. Observers here take the tempo
// and nothing else.
class IGlobalTempoObserver {
public:
    virtual ~IGlobalTempoObserver() = default;
    virtual void onGlobalTempoChanged() = 0;
};

// std::type_identity_t on the second param prevents deduction from nullptr
template<typename T>
inline void swapObserver(T*& stored, std::type_identity_t<T*> next, ITimelineObserver* self)
{
    if (stored) stored->removeObserver(self);
    stored = next;
    if (stored) stored->addObserver(self);
}
