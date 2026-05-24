#include "trackContextPopup.hpp"
#include "appWindow.hpp"
#include "popupStyle.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

static constexpr Fl_Color hoverCol = 0xDDEEFF00;

static ModernButton* makeItem(int y, int w, const char* label)
{
    auto* btn = new ModernButton(1, y, w - 2, TrackContextPopup::btnH, label);
    btn->color(popupBg);
    btn->labelcolor(popupText);
    btn->setHoverColor(hoverCol);
    btn->setBorderWidth(0);
    btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    return btn;
}

TrackContextPopup::TrackContextPopup()
    : BasePopup(0, 0, popW, popH)
{
    color(popupBg);
    box(FL_BORDER_BOX);

    openPatternBtn      = makeItem(1,          popW, "Open Pattern");
    showInstrumentsBtn  = makeItem(1 + btnH,   popW, "Show Instruments");
    addParamBtn         = makeItem(1 + 2*btnH, popW, "Add automation \xe2\x96\xb6");
    addLaneBtn          = makeItem(1 + 3*btnH, popW, "Add Pattern");
    addPianorollLaneBtn = makeItem(1 + 4*btnH, popW, "Add Pianoroll");
    removeLaneBtn       = makeItem(1 + 5*btnH, popW, "Remove Pattern");
    stackLanesBtn       = makeItem(1 + 6*btnH, popW, "Stack Patterns");

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
    stackLanesBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doToggleStackLanes();
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
    stackLanesBtn->onEnter       = hideSubmenuFn;

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
                if (tl.rowOrder[i].isTrack && tl.rowOrder[i].id == laneId) return i;
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
    bool isStacked      = false;
    bool hasMultiLane   = false;
    bool isDrumTrack    = false;
    if (tl) {
        for (const auto& t : tl->get().tracks) {
            if (t.id != trackId) continue;
            hasMultiLane  = (int)t.lanes.size() > 1;
            isStacked     = t.stackedLanes;
            canRemoveLane = hasMultiLane && !isStacked;
            // Determine track type from first lane's pattern
            if (!t.lanes.empty()) {
                int patId = t.lanes[0].patternId;
                for (const auto& p : tl->get().patterns)
                    if (p.id == patId) { isDrumTrack = (p.type == PatternType::DRUM); break; }
            }
            break;
        }
    }
    canRemoveLane ? removeLaneBtn->activate()       : removeLaneBtn->deactivate();
    hasMultiLane  ? stackLanesBtn->activate()       : stackLanesBtn->deactivate();
    isDrumTrack   ? addPianorollLaneBtn->deactivate() : addPianorollLaneBtn->activate();
    stackLanesBtn->label(isStacked ? "Unstack Patterns" : "Stack Patterns");

    position(wx, wy);
    if (auto* aw = dynamic_cast<AppWindow*>(window()))
        aw->openPopup(this);
    else
        show();
    redraw();
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

void TrackContextPopup::doToggleStackLanes()
{
    hide();
    if (!timeline) return;
    bool cur = false;
    for (const auto& t : timeline->get().tracks)
        if (t.id == targetTrackId) { cur = t.stackedLanes; break; }
    timeline->song()->setStackedLanes(targetTrackId, !cur);
    if (auto* win = window()) win->redraw();
}
