#ifndef NOTE_LABELS_CONTEXT_POPUP_HPP
#define NOTE_LABELS_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "parameterSubmenu.hpp"
#include <functional>

class NoteLabelsContextPopup : public ContextMenuPopup {
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

    NoteLabelsContextPopup() : ContextMenuPopup(popW, 2*30+2) {
        auto* addBtn = addItem(0, "Add automation \xe2\x96\xb6");
        removeBtn    = addItem(1, "Remove automation");

        addBtn->callback([](Fl_Widget*, void* d) {
            static_cast<NoteLabelsContextPopup*>(d)->doShowParamSubmenu();
        }, this);
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
        openAt(wx, wy);
    }
};

#endif
