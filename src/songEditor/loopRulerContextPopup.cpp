#include "loopRulerContextPopup.hpp"
#include <FL/Fl.H>

LoopRulerContextPopup::LoopRulerContextPopup()
    : ContextMenuPopup(popW, 2*30+2)
{
    setStartBtn = addItem(0, "Set start");
    setEndBtn   = addItem(1, "Set end");

    setStartBtn->callback([](Fl_Widget*, void* d) {
        auto* p = static_cast<LoopRulerContextPopup*>(d);
        p->hide();
        if (p->setStartFn) p->setStartFn();
    }, this);
    setEndBtn->callback([](Fl_Widget*, void* d) {
        auto* p = static_cast<LoopRulerContextPopup*>(d);
        p->hide();
        if (p->setEndFn) p->setEndFn();
    }, this);

    end();
    hide();
}

void LoopRulerContextPopup::open(int wx, int wy, std::function<void()> onSetStart,
                                 std::function<void()> onSetEnd)
{
    setStartFn = std::move(onSetStart);
    setEndFn   = std::move(onSetEnd);
    openAt(wx, wy);
}
