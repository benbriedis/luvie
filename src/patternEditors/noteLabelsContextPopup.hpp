// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef NOTE_LABELS_CONTEXT_POPUP_HPP
#define NOTE_LABELS_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "parameterSubmenu.hpp"
#include "midiLearn.hpp"
#include <functional>
#include <string>
#include <vector>

class NoteLabelsContextPopup : public ContextMenuPopup {
    std::function<void(const char*)> pendingOnSelect;
    std::function<void()>            pendingOnRemove;
    std::function<void()>            pendingOnRename;
    std::function<bool(const char*)> hasFn_;
    ModernButton*                    renameBtn = nullptr;
    ModernButton*                    addBtn    = nullptr;
    ModernButton*                    removeBtn = nullptr;
    ModernButton*                    editBtn       = nullptr;
    ModernButton*                    learnBtn      = nullptr;
    ModernButton*                    clearLearnBtn = nullptr;
    std::string                      learnType_;
    int                              instrumentId_ = 0;
    std::vector<std::string>         ownNames_;

    void doShowParamSubmenu() {
        if (!paramSubmenu) return;
        paramSubmenu->onLearnNew = nullptr;
        paramSubmenu->onNew      = nullptr;
        auto add = pendingOnSelect;
        auto addLane = [add](const std::string& name) { add(name.c_str()); };
        if (paramActions && paramActions->learnNew && instrumentId_ != 0 && add)
            paramSubmenu->onLearnNew = [this, addLane]() {
                paramActions->learnNew(instrumentId_, addLane);
            };
        if (paramActions && paramActions->create && instrumentId_ != 0 && add)
            paramSubmenu->onNew = [this, addLane](int wx, int wy) {
                paramActions->create(instrumentId_, wx, wy, addLane);
            };
        paramSubmenu->showFor(this, y() + addBtn->y(), ownNames_, hasFn_);
    }

public:
    static constexpr int popW = 160;

    ParameterSubmenu* paramSubmenu = nullptr;
    // Where "MIDI learn" and "Clear MIDI learn" act. Without it they never show.
    MidiLearnMap*     midiLearn    = nullptr;
    // "Edit parameter" and "New (MIDI learn)". Without it they never show.
    ParamMenuActions* paramActions = nullptr;

    NoteLabelsContextPopup() : ContextMenuPopup(popW, 6*30+2) {
        renameBtn = addItem(0, "Rename");
        addBtn    = addItem(1, "Add automation");
        addBtn->setSubmenuArrow(true);
        removeBtn = addItem(2, "Remove automation");
        editBtn       = addItem(3, "Edit parameter");
        learnBtn      = addItem(4, "MIDI learn");
        clearLearnBtn = addItem(5, "Clear MIDI learn");

        editBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<NoteLabelsContextPopup*>(d);
            self->hide();
            if (self->paramActions && self->paramActions->edit)
                self->paramActions->edit(self->instrumentId_, self->learnType_, self->x(), self->y());
        }, this);

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

        learnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<NoteLabelsContextPopup*>(d);
            self->hide();
            if (self->midiLearn) self->midiLearn->toggleLearn(self->learnType_);
        }, this);
        clearLearnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<NoteLabelsContextPopup*>(d);
            self->hide();
            if (self->midiLearn) self->midiLearn->clear(self->learnType_);
        }, this);
        // Moving down onto them from "Add automation" closes its submenu (after a
        // moment: see ParameterSubmenu::hideSoon), as
        // "Remove automation" does for the Song Editor's menu.
        auto closeSubmenu = [this]() { if (paramSubmenu) paramSubmenu->hideSoon(); };
        addBtn->onEnter = [this]() { if (paramSubmenu) paramSubmenu->cancelHide(); };
        removeBtn->onEnter     = closeSubmenu;
        editBtn->onEnter       = closeSubmenu;
        learnBtn->onEnter      = closeSubmenu;
        clearLearnBtn->onEnter = closeSubmenu;

        paramSubmenu = new ParameterSubmenu();
        paramSubmenu->onSelect = [this](const char* type) {
            hide();
            if (pendingOnSelect) pendingOnSelect(type);
        };

        end();
        hide();
    }

    // The instrument the pattern plays and its own parameters, for the parameter
    // submenu and "Edit parameter". Set before each open(); 0 for none.
    void setParamTarget(int instrumentId, std::vector<std::string> ownNames) {
        instrumentId_ = instrumentId;
        ownNames_     = std::move(ownNames);
    }

    void open(int wx, int wy,
              std::function<bool(const char*)> hasFn,
              std::function<void(const char*)> onSelect,
              std::function<void()> onRemove = {},
              std::function<void()> onRename = {},
              std::string learnType = {})
    {
        hasFn_          = std::move(hasFn);
        pendingOnSelect = std::move(onSelect);
        pendingOnRemove = std::move(onRemove);
        pendingOnRename = std::move(onRename);
        learnType_      = std::move(learnType);
        // The MIDI-learn items are for a param lane that was right-clicked.
        const bool canLearn = midiLearn && !learnType_.empty();
        const bool canEdit  = paramActions && paramActions->edit && !learnType_.empty()
                           && instrumentId_ != 0;
        if (canLearn) learnBtn->copy_label(midiLearn->learnMenuLabel(learnType_).c_str());

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
        place(editBtn,       canEdit);
        place(learnBtn,      canLearn);
        place(clearLearnBtn, canLearn && midiLearn->bindingFor(learnType_));

        popH = shown * btnH + 2;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
