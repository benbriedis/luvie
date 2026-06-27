#ifndef LOOP_CONTEXT_POPUP_HPP
#define LOOP_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "observablePattern.hpp"
#include <functional>

// Context menu shown when right-clicking a pattern block in the Loop Editor.
// Kept deliberately thin: the buttons delegate to ObservableSong helpers so the
// menu carries no significant logic of its own. It started as a copy of
// TrackContextPopup and may diverge from it as the Loop Editor needs differ.
class LoopContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 150;

private:
    ModernButton*       openPatternBtn;
    ModernButton*       addLaneBtn;
    ModernButton*       addPianorollLaneBtn;
    ModernButton*       removeLaneBtn;
    ModernButton*       showInstrumentsBtn;
    ObservablePattern*  timeline      = nullptr;
    int                 targetTrackId = -1;
    int                 targetLaneId  = -1;

    void doOpenPattern();
    void doShowInstruments();
    void doAddLane();
    void doAddPianorollLane();
    void doRemoveLane();

public:
    LoopContextPopup();

    std::function<void(int trackIndex, int laneId)> onOpenPattern;
    std::function<void()>                           onShowInstruments;

    void open(int trackId, int laneId, ObservablePattern* tl, int wx, int wy);
};

#endif
