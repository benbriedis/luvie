#ifndef PARAMETER_SUBMENU_HPP
#define PARAMETER_SUBMENU_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "popupStyle.hpp"
#include <functional>

class ParameterSubmenu : public BasePopup {
    static constexpr int numItems  = 5;
    static constexpr Fl_Color hoverCol = 0xDDEEFF00;

public:
    static constexpr int itemH = 30;
    static constexpr int popW  = 140;
    static constexpr int popH  = numItems * itemH + 2;

    std::function<void(const char*)> onSelect;

    ParameterSubmenu() : BasePopup(0, 0, popW, popH) {
        color(popupBg);
        box(FL_BORDER_BOX);

        static const char* labels[] = {
            "Pitch", "Modulation", "Volume", "Pan", "Expression"
        };
        for (int i = 0; i < numItems; ++i) {
            auto* btn = new ModernButton(1, 1 + i*itemH, popW - 2, itemH, labels[i]);
            btn->color(popupBg);
            btn->labelcolor(popupText);
            btn->setHoverColor(hoverCol);
            btn->setBorderWidth(0);
            btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            btn->callback([](Fl_Widget* w, void* d) {
                auto* self = static_cast<ParameterSubmenu*>(d);
                const char* lbl = w->label();
                self->hide();
                if (self->onSelect) self->onSelect(lbl);
            }, this);
        }
        end();
        hide();
    }
};

#endif
