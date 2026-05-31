#ifndef CONTEXT_MENU_POPUP_HPP
#define CONTEXT_MENU_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "../popupStyle.hpp"
#include "../appWindow.hpp"

// Base class for simple button-list context menus.
// Provides styled item creation and standard show/position logic.
class ContextMenuPopup : public BasePopup {
protected:
    static constexpr int      btnH     = 30;
    static constexpr Fl_Color hoverCol = 0xDDEEFF00;
    int popW;

    ModernButton* addItem(int idx, const char* label) {
        auto* btn = new ModernButton(1, 1 + idx * btnH, popW - 2, btnH, label);
        btn->color(popupBg);
        btn->labelcolor(popupText);
        btn->setHoverColor(hoverCol);
        btn->setBorderWidth(0);
        btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        return btn;
    }

    void openAt(int wx, int wy) {
        position(wx, wy);
        if (auto* aw = dynamic_cast<AppWindow*>(window()))
            aw->openPopup(this);
        else
            show();
        redraw();
    }

public:
    ContextMenuPopup(int width, int numItems)
        : BasePopup(0, 0, width, numItems * btnH + 2), popW(width)
    {
        color(popupBg);
        box(FL_BORDER_BOX);
    }
};

#endif
