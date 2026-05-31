#include "trackContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

TrackContextPopup::TrackContextPopup()
    : ContextMenuPopup(popW, 6)
{
    openPatternBtn      = addItem(0, "Open Pattern");
    showInstrumentsBtn  = addItem(1, "Show Instruments");
    addParamBtn         = addItem(2, "Add automation \xe2\x96\xb6");
    addLaneBtn          = addItem(3, "Add Pattern");
    addPianorollLaneBtn = addItem(4, "Add Pianoroll");
    removeLaneBtn       = addItem(5, "Remove Pattern");

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
        if (!timeline || timeline->song()->hasParamLane(type)) return;
        timeline->song()->addParamLane(type, rowOrderIdxForTrackId(targetTrackId) + 1);
    };

    end();
    hide();
}

int TrackContextPopup::rowOrderIdxForTrackId(int trackId) const
{
    if (!timeline) return -1;
    const auto& tl = timeline->get();
    // Find the last lane that has a visible RowRef (handles both normal and stacked mode).
    for (const auto& t : tl.tracks) {
        if (t.id != trackId || t.lanes.empty()) continue;
        for (int li = (int)t.lanes.size() - 1; li >= 0; li--) {
            int laneId = t.lanes[li].id;
            for (int i = (int)tl.rowOrder.size() - 1; i >= 0; i--)
                if (tl.rowOrder[i].kind == RowKind::Lane && tl.rowOrder[i].id == laneId) return i;
        }
        break;
    }
    return -1;
}

void TrackContextPopup::open(int trackId, int laneId, ObservablePattern* tl, int wx, int wy)
{
    timeline      = tl;
    targetTrackId = trackId;
    targetLaneId  = laneId;

    // Configure lane/stack buttons based on track state
    bool canRemoveLane  = false;
    bool canOpenPattern = false;
    bool isDrumTrack    = false;
    if (tl) {
        for (const auto& t : tl->get().tracks) {
            if (t.id != trackId) continue;
            canRemoveLane  = !t.stackedLanes && !t.lanes.empty();
            canOpenPattern = !t.lanes.empty() && !t.stackedLanes;
            // Determine track type from first lane's pattern
            if (!t.lanes.empty()) {
                int patId = t.lanes[0].patternId;
                for (const auto& p : tl->get().patterns)
                    if (p.id == patId) { isDrumTrack = (p.type == PatternType::DRUM); break; }
            }
            break;
        }
    }
    canOpenPattern ? openPatternBtn->activate()        : openPatternBtn->deactivate();
    canRemoveLane  ? removeLaneBtn->activate()         : removeLaneBtn->deactivate();
    isDrumTrack    ? addPianorollLaneBtn->deactivate() : addPianorollLaneBtn->activate();

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

void TrackContextPopup::doShowParamSubmenu()
{
    if (!paramSubmenu) return;
    paramSubmenu->showFor(this, y() + 1 + 2*btnH, timeline->song());
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

