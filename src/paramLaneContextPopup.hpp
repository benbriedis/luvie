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
    ModernButton*      learnBtn      = nullptr;
    ModernButton*      clearLearnBtn = nullptr;

    void doShowParamSubmenu() {
        if (!paramSubmenu) return;
        int instrId = timeline->song()->instrumentIdForParamLane(laneId);
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

    ParamLaneContextPopup() : ContextMenuPopup(popW, 4*30+2) {
        auto* addParamBtn = addItem(0, "Add automation");
        addParamBtn->setSubmenuArrow(true);
        auto* removeBtn   = addItem(1, "Remove automation");
        learnBtn          = addItem(2, "MIDI learn");
        clearLearnBtn     = addItem(3, "Clear MIDI learn");

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

        auto closeSubmenu = [this]() { if (paramSubmenu) paramSubmenu->hide(); };
        removeBtn->onEnter     = closeSubmenu;
        learnBtn->onEnter      = closeSubmenu;
        clearLearnBtn->onEnter = closeSubmenu;

        paramSubmenu = new ParameterSubmenu();
        paramSubmenu->onSelect = [this](const char* type) {
            hide();
            if (!timeline) return;
            int instrId = timeline->song()->instrumentIdForParamLane(laneId);
            if (timeline->song()->hasParamLane(type, instrId)) return;
            // Insert the new lane right after the right-clicked lane in rowOrder.
            int atIndex = -1;
            const auto& ro = timeline->get().rowOrder;
            for (int i = 0; i < (int)ro.size(); i++)
                if (ro[i].kind == RowKind::Param && ro[i].id == laneId) { atIndex = i + 1; break; }
            timeline->song()->addParamLane(type, instrId, atIndex);
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

        // "Clear MIDI learn" only when there is a binding to clear. The rows below
        // the fixed two are stacked, and popH follows, because
        // ContextMenuPopup::resize() snaps the window back to popH.
        const bool canLearn = midiLearn && !laneType.empty();
        int shown = 2;
        auto place = [&](ModernButton* b, bool vis) {
            if (!vis) { b->hide(); return; }
            b->resize(b->x(), 1 + shown * btnH, b->w(), b->h());
            b->show();
            shown++;
        };
        if (canLearn) learnBtn->copy_label(midiLearn->learnMenuLabel(laneType).c_str());
        place(learnBtn,      canLearn);
        place(clearLearnBtn, canLearn && midiLearn->bindingFor(laneType));
        popH = shown * btnH + 2;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
