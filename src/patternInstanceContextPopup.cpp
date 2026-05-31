#include "patternInstanceContextPopup.hpp"
#include "grid.hpp"
#include <FL/Fl.H>

PatternInstanceContextPopup::PatternInstanceContextPopup()
    : ContextMenuPopup(popW, 2*30+2)
{
    auto* openBtn = addItem(0, "Open pattern");
    auto* delBtn  = addItem(1, "Delete");

    openBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (PatternInstanceContextPopup*)me;
        if (self->onOpenPatternFn) self->onOpenPatternFn();
        self->hide();
        if (auto* win = self->window()) win->redraw();
    }, this);

    delBtn->callback([](Fl_Widget*, void* me) {
        auto* self = (PatternInstanceContextPopup*)me;
        if (self->onDeleteFn) self->onDeleteFn();
        self->hide();
        if (auto* win = self->window()) win->redraw();
    }, this);

    end();
    hide();
}

void PatternInstanceContextPopup::open(std::vector<Note>* notes, int noteIdx, Grid* grid,
                                        std::function<void()> onDelete,
                                        std::function<void()> onOpenPattern)
{
    onDeleteFn      = std::move(onDelete);
    onOpenPatternFn = std::move(onOpenPattern);

    Fl_Window* win = grid->window();
    openAt({win->w(), win->h()}, {Fl::event_x(), Fl::event_y()}, 0);
}
