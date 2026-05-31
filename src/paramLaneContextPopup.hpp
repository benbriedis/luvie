#ifndef PARAM_LANE_CONTEXT_POPUP_HPP
#define PARAM_LANE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "observablePattern.hpp"
#include "parameterSubmenu.hpp"

class ParamLaneContextPopup : public ContextMenuPopup {
    ObservablePattern* timeline = nullptr;
    int                laneId   = -1;

    void doShowParamSubmenu() {
        if (!paramSubmenu) return;
        paramSubmenu->showFor(this, y() + 1 + 0*btnH, timeline->song());
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

    ParamLaneContextPopup() : ContextMenuPopup(popW, 2) {
        auto* addParamBtn = addItem(0, "Add automation \xe2\x96\xb6");
        auto* removeBtn   = addItem(1, "Remove automation");

        addParamBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doShowParamSubmenu();
        }, this);
        removeBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doRemove();
        }, this);

        removeBtn->onEnter = [this]() {
            if (paramSubmenu) paramSubmenu->hide();
        };

        paramSubmenu = new ParameterSubmenu();
        paramSubmenu->onSelect = [this](const char* type) {
            hide();
            if (!timeline || timeline->song()->hasParamLane(type)) return;
            int atIndex = -1;
            const auto& lanes = timeline->get().paramLanes;
            for (int i = 0; i < (int)lanes.size(); i++) {
                if (lanes[i].id == laneId) { atIndex = i + 1; break; }
            }
            timeline->song()->addParamLane(type, atIndex);
        };

        end();
        hide();
    }

    void open(int laneId_, ObservablePattern* tl, int wx, int wy) {
        timeline = tl;
        laneId   = laneId_;
        openAt(wx, wy);
    }
};

#endif
