#ifndef PARAMETER_SUBMENU_HPP
#define PARAMETER_SUBMENU_HPP

#include "modern/contextMenuPopup.hpp"
#include "observableSong.hpp"
#include <functional>

class ParameterSubmenu : public ContextMenuPopup {
    static constexpr int numItems  = 5;

    static constexpr const char* typeNames[] = {
        "Pitch", "Modulation", "Volume", "Pan", "Expression"
    };
    static constexpr const char* tickedLabels[] = {
        "\xe2\x9c\x93 Pitch", "\xe2\x9c\x93 Modulation", "\xe2\x9c\x93 Volume",
        "\xe2\x9c\x93 Pan",   "\xe2\x9c\x93 Expression"
    };

    struct ItemData { ParameterSubmenu* self; int idx; };
    ModernButton* items[numItems];
    ItemData      itemData[numItems];

public:
    static constexpr int itemH = btnH;
    static constexpr int popW  = 160;
    static constexpr int popH  = numItems * itemH + 2;

    std::function<void(const char*)> onSelect;

    ParameterSubmenu() : ContextMenuPopup(popW, numItems) {
        for (int i = 0; i < numItems; ++i) {
            itemData[i] = {this, i};
            items[i] = addItem(i, typeNames[i]);
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

    void update(ObservableSong* tl) {
        update([tl](const char* type) { return tl && tl->hasParamLane(type); });
    }

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

    void showFor(BasePopup* parent, int btnY, ObservableSong* tl) {
        showFor(parent, btnY, [tl](const char* type) { return tl && tl->hasParamLane(type); });
    }
};

#endif
