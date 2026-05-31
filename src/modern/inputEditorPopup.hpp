#ifndef INPUT_EDITOR_POPUP_HPP
#define INPUT_EDITOR_POPUP_HPP

#include "contextMenuPopup.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

// Base class for popups that contain an input field and auto-commit on hide.
// Subclasses implement doOk() and call commit() from their doOk()/doDelete().
class InputEditorPopup : public ContextMenuPopup {
protected:
    bool committed = false;

    virtual void doOk() = 0;

    void commit() {
        committed = true;
        Fl_Window::hide();
        if (auto* win = window()) win->redraw();
    }

    void openEditor(int wx, int wy, Fl_Widget* focusTarget) {
        committed = false;
        openAt(wx, wy);
        if (focusTarget) focusTarget->take_focus();
    }

public:
    InputEditorPopup(int w, int h) : ContextMenuPopup(w, h) {}

    int handle(int event) override {
        if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
            doOk(); return 1;
        }
        return ContextMenuPopup::handle(event);
    }

    void hide() override {
        if (!committed && visible()) { doOk(); return; }
        Fl_Window::hide();
    }
};

#endif
