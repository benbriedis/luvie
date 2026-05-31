#ifndef CONTEXT_MENU_POPUP_HPP
#define CONTEXT_MENU_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "../popupStyle.hpp"
#include "../appWindow.hpp"

struct Size   { int width; int height; };
struct Point2 { int x;     int y;      };

inline Point2 calcPopupPos(Size available, Point2 anchor, int anchorH, int popW, int popH) {
    const int vpad = 4, hpad = 10;
    int x = anchor.x;
    if (x + popW > available.width - hpad)  x = available.width - popW - hpad;
    if (x < hpad) x = hpad;
    int y = anchor.y + anchorH + vpad;
    if (y + popH > available.height - vpad) y = anchor.y - popH - vpad;
    if (y < vpad) y = vpad;
    return {x, y};
}

// Base class for all context menu and editor popup windows.
// Provides colour/box setup, item creation, positioning, and show logic.
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

    void openAt(Size available, Point2 anchor, int anchorH) {
        auto pos = calcPopupPos(available, anchor, anchorH, w(), h());
        openAt(pos.x, pos.y);
    }

public:
    ContextMenuPopup(int width, int height)
        : BasePopup(0, 0, width, height), popW(width)
    {
        color(popupBg);
        box(FL_BORDER_BOX);
    }
};

#endif
