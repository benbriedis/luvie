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

    openPatternBtn  = makeItem(1,           popW, "Open Pattern");
    addBtn          = makeItem(1 + btnH,    popW, "Add Track");
    addDrumBtn      = makeItem(1 + 2*btnH,  popW, "Add Drum Track");
    addPianorollBtn = makeItem(1 + 3*btnH,  popW, "Add Pianoroll Track");
    copyBtn         = makeItem(1 + 4*btnH,  popW, "Copy Track");
    deleteBtn       = makeItem(1 + 5*btnH,  popW, "Delete Track");
    addParamBtn     = makeItem(1 + 6*btnH,  popW, "Add automation \xe2\x96\xb6");
    addLaneBtn      = makeItem(1 + 7*btnH,  popW, "Add Pattern");
    removeLaneBtn   = makeItem(1 + 8*btnH,  popW, "Remove Pattern");
    stackLanesBtn   = makeItem(1 + 9*btnH,  popW, "Stack Patterns");

    openPatternBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doOpenPattern();
    }, this);
    addBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAdd();
    }, this);
    addDrumBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAddDrum();
    }, this);
    addPianorollBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAddPianoroll();
    }, this);
    copyBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doCopy();
    }, this);
    deleteBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doDelete();
    }, this);
    addParamBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doShowParamSubmenu();
    }, this);
    addLaneBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAddLane();
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
    openPatternBtn->onEnter  = hideSubmenuFn;
    addBtn->onEnter          = hideSubmenuFn;
    addDrumBtn->onEnter      = hideSubmenuFn;
    addPianorollBtn->onEnter = hideSubmenuFn;
    copyBtn->onEnter         = hideSubmenuFn;
    deleteBtn->onEnter       = hideSubmenuFn;
    addLaneBtn->onEnter      = hideSubmenuFn;
    removeLaneBtn->onEnter   = hideSubmenuFn;
    stackLanesBtn->onEnter   = hideSubmenuFn;

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

    bool canDelete = tl && (int)tl->get().tracks.size() > 1;
    canDelete ? deleteBtn->activate() : deleteBtn->deactivate();

    // Configure lane/stack buttons based on track state
    bool canRemoveLane = false;
    bool isStacked     = false;
    bool hasMultiLane  = false;
    if (tl) {
        for (const auto& t : tl->get().tracks) {
            if (t.id != trackId) continue;
            hasMultiLane  = (int)t.lanes.size() > 1;
            isStacked     = t.stackedLanes;
            // Remove Lane: only available when not stacked (can't pick which lane) and >1 lanes
            canRemoveLane = hasMultiLane && !isStacked;
            break;
        }
    }
    canRemoveLane ? removeLaneBtn->activate() : removeLaneBtn->deactivate();
    hasMultiLane  ? stackLanesBtn->activate() : stackLanesBtn->deactivate();
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

void TrackContextPopup::doAdd()
{
    hide();
    if (!timeline) return;
    int n     = (int)timeline->get().tracks.size() + 1;
    int patId = timeline->createPattern(numPatternBeats);
    timeline->song()->addTrack("T" + std::to_string(n), patId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddDrum()
{
    hide();
    if (!timeline) return;
    int n     = (int)timeline->get().tracks.size() + 1;
    int patId = timeline->createDrumPattern(numPatternBeats);
    timeline->song()->addTrack("T" + std::to_string(n), patId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddPianoroll()
{
    hide();
    if (!timeline) return;
    int n     = (int)timeline->get().tracks.size() + 1;
    int patId = timeline->createPianorollPattern(numPatternBeats);
    timeline->song()->addTrack("T" + std::to_string(n), patId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doCopy()
{
    hide();
    if (!timeline) return;
    int srcPatId = -1;
    for (const auto& t : timeline->get().tracks) {
        if (t.id != targetTrackId) continue;
        srcPatId = t.lanes.empty() ? -1 : t.lanes[0].patternId;
        break;
    }
    if (srcPatId < 0) return;
    int newPatId = timeline->copyPattern(srcPatId);
    if (newPatId < 0) return;
    int n = (int)timeline->get().tracks.size() + 1;
    timeline->song()->addTrack("T" + std::to_string(n), newPatId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doDelete()
{
    hide();
    if (!timeline) return;
    timeline->song()->removeTrackAndPattern(targetTrackId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doShowParamSubmenu()
{
    if (!paramSubmenu) return;
    paramSubmenu->showFor(this, y() + 1 + 6*btnH, timeline->song());
}

void TrackContextPopup::doAddLane()
{
    hide();
    if (!timeline) return;
    timeline->song()->addLane(targetTrackId);
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
