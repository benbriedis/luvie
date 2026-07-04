#include "loopContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

LoopContextPopup::LoopContextPopup()
    : ContextMenuPopup(popW, 6*30+2)
{
    openPatternBtn      = addItem(0, "Open Pattern");
    addLaneBtn          = addItem(1, "Add Pattern");
    addPianorollLaneBtn = addItem(2, "Add Pianoroll");
    cloneLaneBtn        = addItem(3, "Clone Pattern");
    removeLaneBtn       = addItem(4, "Remove Pattern");
    showInstrumentsBtn  = addItem(5, "Show Instruments");

    openPatternBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doOpenPattern();
    }, this);
    addLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doAddLane();
    }, this);
    addPianorollLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doAddPianorollLane();
    }, this);
    cloneLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doCloneLane();
    }, this);
    removeLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doRemoveLane();
    }, this);
    showInstrumentsBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doShowInstruments();
    }, this);

    end();
    hide();
}

void LoopContextPopup::open(int trackId, int laneId, ObservablePattern* tl, int wx, int wy,
                            bool fromLabel)
{
    timeline      = tl;
    targetTrackId = trackId;
    targetLaneId  = laneId;

    auto flags = tl ? tl->song()->trackMenuFlags(trackId)
                    : ObservableSong::TrackMenuFlags{};
    // From a label there is no specific pattern to open or remove.
    bool canOpen   = flags.canOpenPattern && !fromLabel;
    bool canRemove = flags.canRemoveLane  && !fromLabel;
    // Cloning needs a specific pattern, so it follows Open Pattern (disabled
    // from a label) rather than Remove (which is also blocked on last lane).
    bool canClone  = flags.canOpenPattern && !fromLabel;
    canOpen   ? openPatternBtn->activate()  : openPatternBtn->deactivate();
    canClone  ? cloneLaneBtn->activate()    : cloneLaneBtn->deactivate();
    canRemove ? removeLaneBtn->activate()   : removeLaneBtn->deactivate();
    flags.isDrumTrack ? addPianorollLaneBtn->deactivate() : addPianorollLaneBtn->activate();

    openAt(wx, wy);
}

void LoopContextPopup::doOpenPattern()
{
    hide();
    if (onOpenPattern && timeline) {
        int trackIdx = timeline->song()->trackIndexForId(targetTrackId);
        if (trackIdx >= 0) onOpenPattern(trackIdx, targetLaneId);
    }
}

void LoopContextPopup::doShowInstruments()
{
    hide();
    if (onShowInstruments) onShowInstruments();
}

void LoopContextPopup::doAddLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->addLane(targetTrackId);
    if (auto* win = window()) win->redraw();
}

void LoopContextPopup::doAddPianorollLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->addPianorollLane(targetTrackId);
    if (auto* win = window()) win->redraw();
}

void LoopContextPopup::doCloneLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->cloneLane(targetTrackId, targetLaneId);
    if (auto* win = window()) win->redraw();
}

void LoopContextPopup::doRemoveLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->removeLane(targetTrackId, targetLaneId);
    if (auto* win = window()) win->redraw();
}
