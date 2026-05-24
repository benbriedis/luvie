#ifndef PARAM_LANE_CONTEXT_POPUP_HPP
#define PARAM_LANE_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "popupStyle.hpp"
#include "observablePattern.hpp"
#include "parameterSubmenu.hpp"
#include "appWindow.hpp"

class ParamLaneContextPopup : public BasePopup {
    static constexpr int btnH          = 30;
    static constexpr int numBtns       = 2;
    static constexpr int popH          = numBtns * btnH + 2;
    static constexpr Fl_Color hoverCol = 0xDDEEFF00;

    ObservablePattern* timeline = nullptr;
    int                 laneId   = -1;

    static ModernButton* makeItem(int y, int w, const char* label) {
        auto* btn = new ModernButton(1, y, w - 2, btnH, label);
        btn->color(popupBg);
        btn->labelcolor(popupText);
        btn->setHoverColor(hoverCol);
        btn->setBorderWidth(0);
        btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        return btn;
    }

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

    ParamLaneContextPopup() : BasePopup(0, 0, popW, popH) {
        color(popupBg);
        box(FL_BORDER_BOX);

        auto* addParamBtn = makeItem(1,         popW, "Add automation \xe2\x96\xb6");
        auto* removeBtn   = makeItem(1 + btnH,  popW, "Remove automation");

        addParamBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doShowParamSubmenu();
        }, this);
        removeBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doRemove();
        }, this);

        auto hideSubmenuFn = [this]() {
            if (paramSubmenu) paramSubmenu->hide();
        };
        removeBtn->onEnter = hideSubmenuFn;

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
        position(wx, wy);
        if (auto* aw = dynamic_cast<AppWindow*>(window()))
            aw->openPopup(this);
        else
            show();
        redraw();
    }
};

#endif
