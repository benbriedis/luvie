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
    addBtn          = makeItem(1 + btnH,    popW, "Add Pattern");
    addDrumBtn      = makeItem(1 + 2*btnH,  popW, "Add Drum Pattern");
    addPianorollBtn = makeItem(1 + 3*btnH,  popW, "Add Pianoroll Pattern");
    copyBtn         = makeItem(1 + 4*btnH,  popW, "Copy Pattern");
    deleteBtn       = makeItem(1 + 5*btnH,  popW, "Delete Pattern");
    addParamBtn     = makeItem(1 + 6*btnH,  popW, "Add parameter \xe2\x96\xb6");

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
    const auto& ro = timeline->get().rowOrder;
    for (int i = 0; i < (int)ro.size(); i++)
        if (ro[i].isTrack && ro[i].id == trackId) return i;
    return -1;
}

void TrackContextPopup::open(int trackId, ObservablePattern* tl, int wx, int wy)
{
    timeline      = tl;
    targetTrackId = trackId;

    bool canDelete = tl && (int)tl->get().tracks.size() > 1;
    canDelete ? deleteBtn->activate() : deleteBtn->deactivate();

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
        if (trackIdx >= 0) onOpenPattern(trackIdx);
    }
}

void TrackContextPopup::doAdd()
{
    hide();
    if (!timeline) return;
    int n     = timeline->song()->nextTrackNumberForType(PatternType::STANDARD);
    int patId = timeline->createPattern(numPatternBeats);
    timeline->song()->addTrack("Pattern " + std::to_string(n), patId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddDrum()
{
    hide();
    if (!timeline) return;
    int n     = timeline->song()->nextTrackNumberForType(PatternType::DRUM);
    int patId = timeline->createDrumPattern(numPatternBeats);
    timeline->song()->addTrack("Drum " + std::to_string(n), patId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddPianoroll()
{
    hide();
    if (!timeline) return;
    int n     = timeline->song()->nextTrackNumberForType(PatternType::PIANOROLL);
    int patId = timeline->createPianorollPattern(numPatternBeats);
    timeline->song()->addTrack("Pianoroll " + std::to_string(n), patId, rowOrderIdxForTrackId(targetTrackId) + 1);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doCopy()
{
    hide();
    if (!timeline) return;
    int srcPatId = -1;
    PatternType srcType = PatternType::STANDARD;
    for (const auto& t : timeline->get().tracks) {
        if (t.id != targetTrackId) continue;
        srcPatId = t.patternId;
        for (const auto& p : timeline->get().patterns)
            if (p.id == srcPatId) { srcType = p.type; break; }
        break;
    }
    if (srcPatId < 0) return;
    int newPatId = timeline->copyPattern(srcPatId);
    if (newPatId < 0) return;
    int n = timeline->song()->nextTrackNumberForType(srcType);
    static constexpr const char* prefixes[] = { "Pattern", "Drum", "Pianoroll" };
    const char* prefix = prefixes[static_cast<int>(srcType)];
    timeline->song()->addTrack(std::string(prefix) + " " + std::to_string(n), newPatId, rowOrderIdxForTrackId(targetTrackId) + 1);
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
