// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "pasteContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>

PasteContextPopup::PasteContextPopup()
    : ContextMenuPopup(popW, 1*30+2)
{
    auto* pasteBtn = addItem(0, "Paste selection");

    pasteBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (PasteContextPopup*)me;
        // Hide first, as the selection menu's items do: the paste reports a spot
        // it will not fit by putting the forbidden cursor up, which would be
        // hidden behind this menu.
        self->hide();
        if (auto* win = self->window()) win->redraw();
        if (self->onPasteFn) self->onPasteFn();
    }, this);

    end();
    hide();
}

void PasteContextPopup::open(Fl_Widget* owner, std::function<void()> onPaste)
{
    onPasteFn = std::move(onPaste);

    Fl_Window* win = owner->window();
    openAt({win->w(), win->h()}, {Fl::event_x(), Fl::event_y()}, 0);
}
