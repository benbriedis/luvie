#include "loopContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

LoopContextPopup::LoopContextPopup()
    : ContextMenuPopup(popW, 5*30+2)
{
    openPatternBtn      = addItem(0, "Open Pattern");
    addLaneBtn          = addItem(1, "Add Pattern");
    addPianorollLaneBtn = addItem(2, "Add Pianoroll");
    removeLaneBtn       = addItem(3, "Remove Pattern");
    showInstrumentsBtn  = addItem(4, "Show Instruments");

    openPatternBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doOpenPattern();
    }, this);
    addLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doAddLane();
    }, this);
    addPianorollLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doAddPianorollLane();
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
    canOpen   ? openPatternBtn->activate()  : openPatternBtn->deactivate();
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

void LoopContextPopup::doRemoveLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->removeLane(targetTrackId, targetLaneId);
    if (auto* win = window()) win->redraw();
}
