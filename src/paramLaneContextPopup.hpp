#ifndef PARAM_LANE_CONTEXT_POPUP_HPP
#define PARAM_LANE_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "popupStyle.hpp"
#include "observableTimeline.hpp"
#include "parameterSubmenu.hpp"
#include "appWindow.hpp"

class ParamLaneContextPopup : public BasePopup {
    static constexpr int btnH            = 30;
    static constexpr int numBtns         = 5;
    static constexpr int popH            = numBtns * btnH + 2;
    static constexpr int numPatternBeats = 8;
    static constexpr Fl_Color hoverCol   = 0xDDEEFF00;

    ObservableTimeline* timeline = nullptr;
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

    void doAdd() {
        hide();
        if (!timeline) return;
        int n     = (int)timeline->get().tracks.size() + 1;
        int patId = timeline->createPattern(numPatternBeats);
        timeline->addTrack("Pattern " + std::to_string(n), patId);
        if (auto* win = window()) win->redraw();
    }

    void doAddDrum() {
        hide();
        if (!timeline) return;
        int n     = (int)timeline->get().tracks.size() + 1;
        int patId = timeline->createDrumPattern(numPatternBeats);
        timeline->addTrack("Drum " + std::to_string(n), patId);
        if (auto* win = window()) win->redraw();
    }

    void doAddPianoroll() {
        hide();
        if (!timeline) return;
        int n     = (int)timeline->get().tracks.size() + 1;
        int patId = timeline->createPianorollPattern(numPatternBeats);
        timeline->addTrack("Pianoroll " + std::to_string(n), patId);
        if (auto* win = window()) win->redraw();
    }

    void doShowParamSubmenu() {
        if (!paramSubmenu) return;
        int btnY = y() + 1 + 3 * btnH;
        int subX = x() + popW;
        int subY = btnY;
        if (auto* win = window()) {
            int maxX = win->x() + win->w() - ParameterSubmenu::popW;
            int maxY = win->y() + win->h() - ParameterSubmenu::popH;
            if (subX > maxX) subX = x() - ParameterSubmenu::popW;
            if (subY > maxY) subY = maxY;
        }
        paramSubmenu->position(subX, subY);
        paramSubmenu->show();
    }

    void doRemove() {
        hide();
        if (!timeline || laneId < 0) return;
        timeline->removeParamLane(laneId);
        if (auto* win = window()) win->redraw();
    }

public:
    static constexpr int popW = 160;

    ParameterSubmenu* paramSubmenu = nullptr;

    ParamLaneContextPopup() : BasePopup(0, 0, popW, popH) {
        color(popupBg);
        box(FL_BORDER_BOX);

        auto* addBtn          = makeItem(1,           popW, "Add Pattern");
        auto* addDrumBtn      = makeItem(1 + btnH,    popW, "Add Drum Pattern");
        auto* addPianorollBtn = makeItem(1 + 2*btnH,  popW, "Add Pianoroll Pattern");
        auto* addParamBtn     = makeItem(1 + 3*btnH,  popW, "Add parameter \xe2\x96\xb6");
        auto* removeBtn       = makeItem(1 + 4*btnH,  popW, "Remove parameter");

        addBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doAdd();
        }, this);
        addDrumBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doAddDrum();
        }, this);
        addPianorollBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doAddPianoroll();
        }, this);
        addParamBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doShowParamSubmenu();
        }, this);
        removeBtn->callback([](Fl_Widget*, void* d) {
            static_cast<ParamLaneContextPopup*>(d)->doRemove();
        }, this);

        auto hideSubmenuFn = [this]() {
            if (paramSubmenu) paramSubmenu->hide();
        };
        addBtn->onEnter          = hideSubmenuFn;
        addDrumBtn->onEnter      = hideSubmenuFn;
        addPianorollBtn->onEnter = hideSubmenuFn;
        removeBtn->onEnter       = hideSubmenuFn;

        paramSubmenu = new ParameterSubmenu();
        paramSubmenu->onSelect = [this](const char* type) {
            hide();
            if (timeline && !timeline->hasParamLane(type))
                timeline->addParamLane(type);
        };

        end();
        hide();
    }

    void open(int laneId_, ObservableTimeline* tl, int wx, int wy) {
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
