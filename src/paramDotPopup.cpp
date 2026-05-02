#include "paramDotPopup.hpp"
#include "appWindow.hpp"
#include "popupStyle.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>

static constexpr int popupW = 140;
static constexpr int popupH = 70;
static constexpr int pad    = 8;
static constexpr int row1Y  = pad;
static constexpr int row1H  = 22;
static constexpr int row2Y  = pad + row1H + pad;
static constexpr int row2H  = 24;

ParamDotPopup::ParamDotPopup()
    : BasePopup(0, 0, popupW, popupH)
{
    color(popupBg);
    box(FL_BORDER_BOX);

    auto* lbl = new Fl_Box(pad, row1Y, 26, row1H, "Val");
    lbl->labelcolor(popupText);
    lbl->box(FL_NO_BOX);

    input = new Fl_Value_Input(pad + 30, row1Y, popupW - pad - 30 - pad, row1H);
    input->range(0, 16383);
    input->step(1);
    input->box(FL_FLAT_BOX);
    input->color(popupInputBg);
    input->textcolor(popupText);
    input->cursor_color(popupText);
    input->labelcolor(popupText);

    deleteBtn = new ModernButton(pad, row2Y, popupW - 2 * pad, row2H, "Delete");
    deleteBtn->color(FL_WHITE);
    deleteBtn->labelcolor(popupText);

    end();

    deleteBtn->callback([](Fl_Widget*, void* d) {
        static_cast<ParamDotPopup*>(d)->doDelete();
    }, this);
}

void ParamDotPopup::doOk()
{
    committed = true;
    if (onOkCb) onOkCb((int)input->value());
    hide();
    if (auto* win = window()) win->redraw();
}

void ParamDotPopup::doDelete()
{
    committed = true;
    if (onDeleteCb) onDeleteCb();
    hide();
    if (auto* win = window()) win->redraw();
}

void ParamDotPopup::hide()
{
    if (!committed && visible()) {
        doOk();
        return;
    }
    Fl_Window::hide();
}

int ParamDotPopup::handle(int event)
{
    if (event == FL_KEYDOWN) {
        if (Fl::event_key() == FL_Enter) { doOk(); return 1; }
    }
    return BasePopup::handle(event);
}

void ParamDotPopup::open(int wx, int wy, int value, bool isAnchor, int maxVal,
                         std::function<void(int)> onOk,
                         std::function<void()>    onDelete)
{
    committed  = false;
    input->range(0, maxVal);
    input->value(value);
    isAnchor ? deleteBtn->deactivate() : deleteBtn->activate();
    onOkCb     = std::move(onOk);
    onDeleteCb = std::move(onDelete);
    position(wx, wy);
    if (auto* aw = dynamic_cast<AppWindow*>(window()))
        aw->openPopup(this);
    else
        show();
    redraw();
    input->take_focus();
}
