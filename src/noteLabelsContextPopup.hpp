#ifndef NOTE_LABELS_CONTEXT_POPUP_HPP
#define NOTE_LABELS_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "parameterSubmenu.hpp"
#include "popupStyle.hpp"
#include "appWindow.hpp"
#include <functional>

class NoteLabelsContextPopup : public BasePopup {
    static constexpr int btnH     = 30;
    static constexpr int popH     = btnH + 2;
    static constexpr Fl_Color hoverCol = 0xDDEEFF00;

    std::function<void(const char*)> pendingOnSelect;
    std::function<bool(const char*)> hasFn_;

    void doShowParamSubmenu() {
        if (paramSubmenu) paramSubmenu->showFor(this, y() + 1, hasFn_);
    }

public:
    static constexpr int popW = 160;

    ParameterSubmenu* paramSubmenu = nullptr;

    NoteLabelsContextPopup() : BasePopup(0, 0, popW, popH) {
        color(popupBg);
        box(FL_BORDER_BOX);

        auto* btn = new ModernButton(1, 1, popW - 2, btnH, "Add parameter \xe2\x96\xb6");
        btn->color(popupBg);
        btn->labelcolor(popupText);
        btn->setHoverColor(hoverCol);
        btn->setBorderWidth(0);
        btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        btn->callback([](Fl_Widget*, void* d) {
            static_cast<NoteLabelsContextPopup*>(d)->doShowParamSubmenu();
        }, this);

        paramSubmenu = new ParameterSubmenu();
        paramSubmenu->onSelect = [this](const char* type) {
            hide();
            if (pendingOnSelect) pendingOnSelect(type);
        };

        end();
        hide();
    }

    void open(int wx, int wy,
              std::function<bool(const char*)> hasFn,
              std::function<void(const char*)> onSelect)
    {
        hasFn_          = std::move(hasFn);
        pendingOnSelect = std::move(onSelect);
        position(wx, wy);
        if (auto* aw = dynamic_cast<AppWindow*>(window()))
            aw->openPopup(this);
        else
            show();
        redraw();
    }
};

#endif
