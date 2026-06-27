#include "trackContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

TrackContextPopup::TrackContextPopup()
    : ContextMenuPopup(popW, 6*30+2)
{
    openPatternBtn      = addItem(0, "Open Pattern");
    addParamBtn         = addItem(1, "Add automation \xe2\x96\xb6");
    addLaneBtn          = addItem(2, "Add Pattern");
    addPianorollLaneBtn = addItem(3, "Add Pianoroll");
    removeLaneBtn       = addItem(4, "Remove Pattern");
    showInstrumentsBtn  = addItem(5, "Show Instruments");

    openPatternBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doOpenPattern();
    }, this);
    showInstrumentsBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doShowInstruments();
    }, this);
    addParamBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doShowParamSubmenu();
    }, this);
    addLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAddLane();
    }, this);
    addPianorollLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAddPianorollLane();
    }, this);
    removeLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doRemoveLane();
    }, this);

    // Hovering any non-submenu button hides the parameter submenu.
    auto hideSubmenuFn = [this]() {
        if (paramSubmenu) paramSubmenu->hide();
    };
    openPatternBtn->onEnter      = hideSubmenuFn;
    showInstrumentsBtn->onEnter  = hideSubmenuFn;
    addLaneBtn->onEnter          = hideSubmenuFn;
    addPianorollLaneBtn->onEnter = hideSubmenuFn;
    removeLaneBtn->onEnter       = hideSubmenuFn;

    paramSubmenu = new ParameterSubmenu();
    paramSubmenu->onSelect = [this](const char* type) {
        hide();
        if (!timeline) return;
        int instrId = targetInstrumentId();
        if (timeline->song()->hasParamLane(type, instrId)) return;
        timeline->song()->addParamLane(type, instrId,
                                       timeline->song()->paramLaneInsertIndex(targetTrackId));
    };

    end();
    hide();
}

void TrackContextPopup::open(int trackId, int laneId, ObservablePattern* tl, int wx, int wy)
{
    timeline      = tl;
    targetTrackId = trackId;
    targetLaneId  = laneId;

    auto flags = tl ? tl->song()->trackMenuFlags(trackId)
                    : ObservableSong::TrackMenuFlags{};
    flags.canOpenPattern ? openPatternBtn->activate()      : openPatternBtn->deactivate();
    flags.canRemoveLane  ? removeLaneBtn->activate()       : removeLaneBtn->deactivate();
    flags.isDrumTrack    ? addPianorollLaneBtn->deactivate(): addPianorollLaneBtn->activate();

    openAt(wx, wy);
}

void TrackContextPopup::doOpenPattern()
{
    hide();
    if (onOpenPattern && timeline) {
        int trackIdx = timeline->song()->trackIndexForId(targetTrackId);
        if (trackIdx >= 0) onOpenPattern(trackIdx, targetLaneId);
    }
}

void TrackContextPopup::doShowInstruments()
{
    hide();
    if (onShowInstruments) onShowInstruments();
}

int TrackContextPopup::targetInstrumentId() const
{
    return timeline ? timeline->song()->instrumentIdForTrack(targetTrackId) : 0;
}

void TrackContextPopup::doShowParamSubmenu()
{
    if (!paramSubmenu) return;
    paramSubmenu->showFor(this, y() + 1 + 1*btnH, timeline->song(), targetInstrumentId());
}

void TrackContextPopup::doAddLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->addLane(targetTrackId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddPianorollLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->addPianorollLane(targetTrackId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doRemoveLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->removeLane(targetTrackId, targetLaneId);
    if (auto* win = window()) win->redraw();
}

