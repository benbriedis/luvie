#ifndef TOGGLE_BUTTON_HPP
#define TOGGLE_BUTTON_HPP

#include "modernButton.hpp"
#include <functional>

// A ModernButton that flips between two fixed text labels on each click and
// reports the new state via onToggle(bool). Shared by the #/b (sharp/flat) and
// Chord/Scale switches in the pattern control bar.
class ToggleButton : public ModernButton {
    const char* onLabel;
    const char* offLabel;
    bool        on;

public:
    std::function<void(bool)> onToggle;

    ToggleButton(int x, int y, int w, int h,
                 const char* onLabel, const char* offLabel, bool on = true)
        : ModernButton(x, y, w, h, on ? onLabel : offLabel),
          onLabel(onLabel), offLabel(offLabel), on(on)
    {
        callback([](Fl_Widget* w, void*) {
            auto* self = static_cast<ToggleButton*>(w);
            self->set(!self->on);
            if (self->onToggle) self->onToggle(self->on);
        });
    }

    bool isOn() const { return on; }

    // Set the state without firing onToggle (used to reflect external changes).
    void set(bool state) {
        on = state;
        label(on ? onLabel : offLabel);
        redraw();
    }
};

#endif
