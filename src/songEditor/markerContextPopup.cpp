#include "markerContextPopup.hpp"

MarkerContextPopup::MarkerContextPopup()
    : ContextMenuPopup(popW, btnH + 2)
{
    addBtn = addItem(0, "");

    addBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<MarkerContextPopup*>(d);
        self->hide();
        if (self->onAddCb) self->onAddCb();
    }, this);

    end();
    hide();
}

void MarkerContextPopup::open(const char* label, std::function<void()> onAdd, int wx, int wy)
{
    addBtn->copy_label(label);
    onAddCb = std::move(onAdd);
    openAt(wx, wy);
}
