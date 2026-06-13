#ifndef INLINE_INPUT_HPP
#define INLINE_INPUT_HPP

#include <FL/Fl_Input.H>
#include <functional>

// Fl_Input subclass that fires callbacks when focus leaves it or the value changes.
class InlineInput : public Fl_Input {
    std::function<void()> unfocusCb;
    std::function<void()> changeCb;
public:
    InlineInput(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {}
    void onUnfocus(std::function<void()> cb) { unfocusCb = std::move(cb); }
    void onChange(std::function<void()> cb)  { changeCb  = std::move(cb); }
    int handle(int event) override {
        int r = Fl_Input::handle(event);
        if (event == FL_UNFOCUS  && unfocusCb) unfocusCb();
        if (event == FL_KEYBOARD && changeCb)  changeCb();
        return r;
    }
};

#endif
