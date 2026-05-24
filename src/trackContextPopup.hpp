#ifndef TRACK_CONTEXT_POPUP_HPP
#define TRACK_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "observablePattern.hpp"
#include "parameterSubmenu.hpp"
#include <functional>

class TrackContextPopup : public BasePopup {
    static constexpr int popH  = 10 * 30 + 2;  // +2 for border

public:
    static constexpr int popW  = 150;
    static constexpr int btnH  = 30;

private:

    ModernButton*       openPatternBtn;
    ModernButton*       addBtn;
    ModernButton*       addDrumBtn;
    ModernButton*       addPianorollBtn;
    ModernButton*       copyBtn;
    ModernButton*       deleteBtn;
    ModernButton*       addParamBtn;
    ModernButton*       addLaneBtn;
    ModernButton*       removeLaneBtn;
    ModernButton*       stackLanesBtn;
    ObservablePattern*  timeline      = nullptr;
    int                 targetTrackId = -1;
    int                 targetLaneId  = -1;

    static constexpr int numPatternBeats = 8;

    int  rowOrderIdxForTrackId(int trackId) const;
    void doOpenPattern();
    void doAdd();
    void doAddDrum();
    void doAddPianoroll();
    void doCopy();
    void doDelete();
    void doShowParamSubmenu();
    void doAddLane();
    void doRemoveLane();
    void doToggleStackLanes();

public:
    ParameterSubmenu*                    paramSubmenu = nullptr;

    TrackContextPopup();

    std::function<void(int trackIndex, int laneId)> onOpenPattern;

    void open(int trackId, int laneId, ObservablePattern* tl, int wx, int wy);
};

#endif
