#ifndef BASE_POPUP_HPP
#define BASE_POPUP_HPP

#include <FL/Fl_Window.H>

// Common base for all popup windows. Ensures clicks in gaps between child
// widgets are never propagated to widgets underneath the popup.
class BasePopup : public Fl_Window {
public:
    using Fl_Window::Fl_Window;

    int handle(int event) override {
        int r = Fl_Window::handle(event);
        if (event == FL_PUSH) return 1;  // always consume clicks
        return r;
    }
};

#endif
