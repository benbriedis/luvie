// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef NOTE_LABELS_CONTEXT_POPUP_HPP
#define NOTE_LABELS_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "parameterSubmenu.hpp"
#include <functional>

class NoteLabelsContextPopup : public ContextMenuPopup {
    std::function<void(const char*)> pendingOnSelect;
    std::function<void()>            pendingOnRemove;
    std::function<void()>            pendingOnRename;
    std::function<bool(const char*)> hasFn_;
    ModernButton*                    renameBtn = nullptr;
    ModernButton*                    addBtn    = nullptr;
    ModernButton*                    removeBtn = nullptr;

    void doShowParamSubmenu() {
        if (paramSubmenu) paramSubmenu->showFor(this, y() + addBtn->y(), hasFn_);
    }

public:
    static constexpr int popW = 160;

    ParameterSubmenu* paramSubmenu = nullptr;

    NoteLabelsContextPopup() : ContextMenuPopup(popW, 3*30+2) {
        renameBtn = addItem(0, "Rename");
        addBtn    = addItem(1, "Add automation \xe2\x96\xb6");
        removeBtn = addItem(2, "Remove automation");

        renameBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<NoteLabelsContextPopup*>(d);
            self->hide();
            if (self->pendingOnRename) self->pendingOnRename();
        }, this);
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
              std::function<void()> onRemove = {},
              std::function<void()> onRename = {})
    {
        hasFn_          = std::move(hasFn);
        pendingOnSelect = std::move(onSelect);
        pendingOnRemove = std::move(onRemove);
        pendingOnRename = std::move(onRename);

        // Stack the visible rows from the top with no gaps. "Rename" only shows
        // for editors that supply a rename handler (the drum editor); "Remove
        // automation" only when there's an existing param lane to remove.
        // popH must be updated too: ContextMenuPopup::resize() snaps the window
        // back to popH, so size() alone won't stick.
        int shown = 0;
        auto place = [&](ModernButton* b, bool vis) {
            if (!b) return;
            if (!vis) { b->hide(); return; }
            b->resize(b->x(), 1 + shown * btnH, b->w(), b->h());
            b->show();
            shown++;
        };
        place(renameBtn, (bool)pendingOnRename);
        place(addBtn,    true);
        place(removeBtn, (bool)pendingOnRemove);

        popH = shown * btnH + 2;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
