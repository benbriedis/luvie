#ifndef PARAMETER_SUBMENU_HPP
#define PARAMETER_SUBMENU_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "observableTimeline.hpp"
#include "popupStyle.hpp"
#include <functional>

class ParameterSubmenu : public BasePopup {
    static constexpr int numItems  = 5;
    static constexpr Fl_Color hoverCol = 0xDDEEFF00;

    static constexpr const char* typeNames[] = {
        "Pitch", "Modulation", "Volume", "Pan", "Expression"
    };
    // UTF-8 check mark (U+2713) prefix for already-present parameters.
    static constexpr const char* tickedLabels[] = {
        "\xe2\x9c\x93 Pitch", "\xe2\x9c\x93 Modulation", "\xe2\x9c\x93 Volume",
        "\xe2\x9c\x93 Pan",   "\xe2\x9c\x93 Expression"
    };

    struct ItemData { ParameterSubmenu* self; int idx; };
    ModernButton* items[numItems];
    ItemData      itemData[numItems];

public:
    static constexpr int itemH = 30;
    static constexpr int popW  = 160;
    static constexpr int popH  = numItems * itemH + 2;

    std::function<void(const char*)> onSelect;

    ParameterSubmenu() : BasePopup(0, 0, popW, popH) {
        color(popupBg);
        box(FL_BORDER_BOX);

        for (int i = 0; i < numItems; ++i) {
            itemData[i] = {this, i};
            items[i] = new ModernButton(1, 1 + i*itemH, popW - 2, itemH, typeNames[i]);
            items[i]->color(popupBg);
            items[i]->labelcolor(popupText);
            items[i]->setHoverColor(hoverCol);
            items[i]->setBorderWidth(0);
            items[i]->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            items[i]->callback([](Fl_Widget*, void* d) {
                auto* data = static_cast<ItemData*>(d);
                data->self->hide();
                if (data->self->onSelect) data->self->onSelect(typeNames[data->idx]);
            }, &itemData[i]);
        }
        end();
        hide();
    }

    void update(const std::function<bool(const char*)>& hasFn) {
        for (int i = 0; i < numItems; ++i) {
            bool has = hasFn(typeNames[i]);
            items[i]->label(has ? tickedLabels[i] : typeNames[i]);
            has ? items[i]->deactivate() : items[i]->activate();
        }
        redraw();
    }

    void update(ObservableTimeline* tl) {
        update([tl](const char* type) { return tl && tl->hasParamLane(type); });
    }

    // Update state and show the submenu positioned to the right of `parent` at `btnY`,
    // clamping to stay inside the parent window.
    void showFor(BasePopup* parent, int btnY, const std::function<bool(const char*)>& hasFn) {
        update(hasFn);
        int subX = parent->x() + parent->w();
        int subY = btnY;
        if (auto* win = parent->window()) {
            if (subX > win->x() + win->w() - popW) subX = parent->x() - popW;
            int maxY = win->y() + win->h() - popH;
            if (subY > maxY) subY = maxY;
        }
        position(subX, subY);
        show();
    }

    void showFor(BasePopup* parent, int btnY, ObservableTimeline* tl) {
        showFor(parent, btnY, [tl](const char* type) { return tl && tl->hasParamLane(type); });
    }
};

#endif
