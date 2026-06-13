#ifndef TRACK_CONTEXT_POPUP_HPP
#define TRACK_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "observablePattern.hpp"
#include "parameterSubmenu.hpp"
#include <functional>

class TrackContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW  = 150;

private:

    ModernButton*       openPatternBtn;
    ModernButton*       showInstrumentsBtn;
    ModernButton*       addParamBtn;
    ModernButton*       addLaneBtn;
    ModernButton*       addPianorollLaneBtn;
    ModernButton*       removeLaneBtn;
    ObservablePattern*  timeline      = nullptr;
    int                 targetTrackId = -1;
    int                 targetLaneId  = -1;

    int  rowOrderIdxForTrackId(int trackId) const;
    int  targetInstrumentId() const;
    void doOpenPattern();
    void doShowInstruments();
    void doShowParamSubmenu();
    void doAddLane();
    void doAddPianorollLane();
    void doRemoveLane();

public:
    ParameterSubmenu*                    paramSubmenu = nullptr;

    TrackContextPopup();

    std::function<void(int trackIndex, int laneId)> onOpenPattern;
    std::function<void()>                           onShowInstruments;

    void open(int trackId, int laneId, ObservablePattern* tl, int wx, int wy);
};

#endif
