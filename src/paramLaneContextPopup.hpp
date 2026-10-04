// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PARAM_LANE_CONTEXT_POPUP_HPP
#define PARAM_LANE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "observablePattern.hpp"
#include "parameterSubmenu.hpp"
#include "midiLearn.hpp"
#include <string>

class ParamLaneContextPopup : public ContextMenuPopup {
    ObservablePattern* timeline = nullptr;
    int                laneId   = -1;
    std::string        laneType;          // what MIDI learn binds
    ModernButton*      editBtn       = nullptr;
    ModernButton*      learnBtn      = nullptr;
    ModernButton*      clearLearnBtn = nullptr;

    // Where a lane added from this menu goes: right after the right-clicked one.
    int insertIndex() const {
        const auto& ro = timeline->get().rowOrder;
        for (int i = 0; i < (int)ro.size(); i++)
            if (ro[i].kind == RowKind::Param && ro[i].id == laneId) return i + 1;
        return -1;
    }

    void doShowParamSubmenu() {
        if (!paramSubmenu) return;
        int instrId = timeline->song()->instrumentIdForParamLane(laneId);
        paramSubmenu->onLearnNew = nullptr;
        paramSubmenu->onNew      = nullptr;
        ObservablePattern* tl = timeline;
        const int at = insertIndex();
        auto addLane = [tl, instrId, at](const std::string& name) {
            if (!tl || tl->song()->hasParamLane(name, instrId)) return;
            tl->song()->addParamLane(name, instrId, at);
        };
        if (paramActions && paramActions->learnNew && instrId != 0)
            paramSubmenu->onLearnNew = [this, instrId, addLane]() {
                paramActions->learnNew(instrId, addLane);
            };
        if (paramActions && paramActions->create && instrId != 0)
            paramSubmenu->onNew = [this, instrId, addLane](int wx, int wy) {
                paramActions->create(instrId, wx, wy, addLane);
            };
        paramSubmenu->showFor(this, y() + 1 + 0*btnH, timeline->song(), instrId);
    }

    void doRemove() {
        hide();
        if (!timeline || laneId < 0) return;
        timeline->song()->removeParamLane(laneId);
        if (auto* win = window()) win->redraw();
    }

public:
    static constexpr int popW = 160;

    ParameterSubmenu* paramSubmenu = nullptr;
    // Where "MIDI learn" and "Clear MIDI learn" act. Without it they never show.
    MidiLearnMap*     midiLearn    = nullptr;
    // "Edit parameter" and "New (MIDI learn)". Without it they never show.
    ParamMenuActions* paramActions = nullptr;

    ParamLaneContextPopup() : ContextMenuPopup(popW, 5*30+2) {
        auto* addParamBtn = addItem(0, "Add automation");
        addParamBtn->setSubmenuArrow(true);
        auto* removeBtn   = addItem(1, "Remove automation");
        editBtn           = addItem(2, "Edit parameter");
        learnBtn          = addItem(3, "MIDI learn");
        clearLearnBtn     = addItem(4, "Clear MIDI learn");

        editBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<ParamLaneContextPopup*>(d);
            self->hide();
            if (!self->timeline || !self->paramActions || !self->paramActions->edit) return;
            const int instrId = self->timeline->song()->instrumentIdForParamLane(self->laneId);
            self->paramActions->edit(instrId, self->laneType, self->x(), self->y());
        }, this);

        addParamBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doShowParamSubmenu();
        }, this);
        removeBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doRemove();
        }, this);

        learnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<ParamLaneContextPopup*>(d);
            self->hide();
            if (self->midiLearn) self->midiLearn->toggleLearn(self->laneType);
        }, this);
        clearLearnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<ParamLaneContextPopup*>(d);
            self->hide();
            if (self->midiLearn) self->midiLearn->clear(self->laneType);
        }, this);

        auto closeSubmenu = [this]() { if (paramSubmenu) paramSubmenu->hideSoon(); };
        addParamBtn->onEnter = [this]() { if (paramSubmenu) paramSubmenu->cancelHide(); };
        removeBtn->onEnter     = closeSubmenu;
        editBtn->onEnter       = closeSubmenu;
        learnBtn->onEnter      = closeSubmenu;
        clearLearnBtn->onEnter = closeSubmenu;

        paramSubmenu = new ParameterSubmenu();
        paramSubmenu->onSelect = [this](const char* type) {
            hide();
            if (!timeline) return;
            int instrId = timeline->song()->instrumentIdForParamLane(laneId);
            if (timeline->song()->hasParamLane(type, instrId)) return;
            timeline->song()->addParamLane(type, instrId, insertIndex());
        };

        end();
        hide();
    }

    void open(int laneId_, ObservablePattern* tl, int wx, int wy) {
        timeline = tl;
        laneId   = laneId_;
        laneType.clear();
        if (tl)
            for (const auto& l : tl->get().paramLanes)
                if (l.id == laneId) { laneType = l.type; break; }

        // "Clear MIDI learn" only when there is a binding to clear, and "Edit
        // parameter" only for a lane with an instrument. The rows below the fixed
        // two are stacked, and popH follows, because
        // ContextMenuPopup::resize() snaps the window back to popH.
        const bool canLearn = midiLearn && !laneType.empty();
        const bool canEdit  = paramActions && paramActions->edit && !laneType.empty() && tl
                           && tl->song()->instrumentIdForParamLane(laneId) != 0;
        int shown = 2;
        auto place = [&](ModernButton* b, bool vis) {
            if (!vis) { b->hide(); return; }
            b->resize(b->x(), 1 + shown * btnH, b->w(), b->h());
            b->show();
            shown++;
        };
        if (canLearn) learnBtn->copy_label(midiLearn->learnMenuLabel(laneType).c_str());
        place(editBtn,       canEdit);
        place(learnBtn,      canLearn);
        place(clearLearnBtn, canLearn && midiLearn->bindingFor(laneType));
        popH = shown * btnH + 2;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
