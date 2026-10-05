// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "selectionContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>

SelectionContextPopup::SelectionContextPopup()
    : ContextMenuPopup(popW, 4*btnH+2)
{
    auto* cutBtn  = addItem(0, "Cut selection");
    auto* copyBtn = addItem(1, "Copy selection");
    auto* delBtn  = addItem(2, "Delete selection");
    delSliceBtn   = addItem(3, "Delete slice");

    cutBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (SelectionContextPopup*)me;
        // Hide first, as copying does: cutting is a copy and the paste that
        // follows needs the grid live again.
        self->hide();
        if (auto* win = self->window()) win->redraw();
        if (self->onCutFn) self->onCutFn();
    }, this);

    copyBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (SelectionContextPopup*)me;
        // Hide before copying rather than after, so the grid is live again by
        // the time the paste that follows can arrive.
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

    delSliceBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (SelectionContextPopup*)me;
        if (self->onDeleteSliceFn) self->onDeleteSliceFn();
        self->hide();
        if (auto* win = self->window()) win->redraw();
    }, this);

    end();
    hide();
}

void SelectionContextPopup::open(Fl_Widget* owner, std::function<void()> onCut,
                                                  std::function<void()> onCopy,
                                                  std::function<void()> onDelete,
                                                  std::function<void()> onDeleteSlice)
{
    onCutFn         = std::move(onCut);
    onCopyFn        = std::move(onCopy);
    onDeleteFn      = std::move(onDelete);
    onDeleteSliceFn = std::move(onDeleteSlice);

    // Sized before positioning, so the menu is kept on screen at its real height.
    if (onDeleteSliceFn) delSliceBtn->show(); else delSliceBtn->hide();
    popH = (onDeleteSliceFn ? 4 : 3) * btnH + 2;
    size(popW, popH);

    Fl_Window* win = owner->window();
    openAt({win->w(), win->h()}, {Fl::event_x(), Fl::event_y()}, 0);
}
