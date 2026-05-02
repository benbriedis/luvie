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
    static constexpr int popH     = 2 * btnH + 2;
    static constexpr Fl_Color hoverCol = 0xDDEEFF00;

    std::function<void(const char*)> pendingOnSelect;
    std::function<void()>            pendingOnRemove;
    std::function<bool(const char*)> hasFn_;
    ModernButton*                    removeBtn = nullptr;

    void doShowParamSubmenu() {
        if (paramSubmenu) paramSubmenu->showFor(this, y() + 1, hasFn_);
    }

public:
    static constexpr int popW = 160;

    ParameterSubmenu* paramSubmenu = nullptr;

    NoteLabelsContextPopup() : BasePopup(0, 0, popW, popH) {
        color(popupBg);
        box(FL_BORDER_BOX);

        auto* addBtn = new ModernButton(1, 1, popW - 2, btnH, "Add parameter \xe2\x96\xb6");
        addBtn->color(popupBg);
        addBtn->labelcolor(popupText);
        addBtn->setHoverColor(hoverCol);
        addBtn->setBorderWidth(0);
        addBtn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        addBtn->callback([](Fl_Widget*, void* d) {
            static_cast<NoteLabelsContextPopup*>(d)->doShowParamSubmenu();
        }, this);

        removeBtn = new ModernButton(1, 1 + btnH, popW - 2, btnH, "Remove parameter");
        removeBtn->color(popupBg);
        removeBtn->labelcolor(popupText);
        removeBtn->setHoverColor(hoverCol);
        removeBtn->setBorderWidth(0);
        removeBtn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        removeBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<NoteLabelsContextPopup*>(d);
            self->hide();
            if (self->pendingOnRemove) self->pendingOnRemove();
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
              std::function<void(const char*)> onSelect,
              std::function<void()> onRemove = {})
    {
        hasFn_          = std::move(hasFn);
        pendingOnSelect = std::move(onSelect);
        pendingOnRemove = std::move(onRemove);
        if (removeBtn) {
            if (pendingOnRemove)
                removeBtn->activate();
            else
                removeBtn->deactivate();
        }
        position(wx, wy);
        if (auto* aw = dynamic_cast<AppWindow*>(window()))
            aw->openPopup(this);
        else
            show();
        redraw();
    }
};

#endif
