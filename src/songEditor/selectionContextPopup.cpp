// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "selectionContextPopup.hpp"
#include "grid.hpp"
#include <FL/Fl.H>

SelectionContextPopup::SelectionContextPopup()
    : ContextMenuPopup(popW, 2*30+2)
{
    auto* copyBtn = addItem(0, "Copy selection");
    auto* delBtn  = addItem(1, "Delete selection");

    copyBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (SelectionContextPopup*)me;
        // Hide first: copying starts a ghost that follows the cursor, and the
        // grid is deaf while this menu is up.
        self->hide();
        if (auto* win = self->window()) win->redraw();
        if (self->onCopyFn) self->onCopyFn();
    }, this);

    delBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (SelectionContextPopup*)me;
        if (self->onDeleteFn) self->onDeleteFn();
        self->hide();
        if (auto* win = self->window()) win->redraw();
    }, this);

    end();
    hide();
}

void SelectionContextPopup::open(Grid* grid, std::function<void()> onCopy,
                                             std::function<void()> onDelete)
{
    onCopyFn   = std::move(onCopy);
    onDeleteFn = std::move(onDelete);

    Fl_Window* win = grid->window();
    openAt({win->w(), win->h()}, {Fl::event_x(), Fl::event_y()}, 0);
}
