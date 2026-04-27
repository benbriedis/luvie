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
        if (onAddParameter) onAddParameter(type);
    };

    end();
    hide();
}

void TrackContextPopup::open(int row, ObservableTimeline* tl, int wx, int wy)
{
    timeline  = tl;
    targetRow = row;

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
    if (onOpenPattern) onOpenPattern(targetRow);
}

void TrackContextPopup::doAdd()
{
    hide();
    if (!timeline) return;
    int n     = (int)timeline->get().tracks.size() + 1;
    int patId = timeline->createPattern(numPatternBeats);
    timeline->addTrack("Pattern " + std::to_string(n), patId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddDrum()
{
    hide();
    if (!timeline) return;
    int n     = (int)timeline->get().tracks.size() + 1;
    int patId = timeline->createDrumPattern(numPatternBeats);
    timeline->addTrack("Drum " + std::to_string(n), patId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doAddPianoroll()
{
    hide();
    if (!timeline) return;
    int n     = (int)timeline->get().tracks.size() + 1;
    int patId = timeline->createPianorollPattern(numPatternBeats);
    timeline->addTrack("Pianoroll " + std::to_string(n), patId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doCopy()
{
    hide();
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    if (targetRow < 0 || targetRow >= (int)tracks.size()) return;
    int srcPatId = tracks[targetRow].patternId;
    int newPatId = timeline->copyPattern(srcPatId);
    if (newPatId < 0) return;
    int n = (int)tracks.size() + 1;
    timeline->addTrack("Pattern " + std::to_string(n), newPatId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doDelete()
{
    hide();
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    if (targetRow < 0 || targetRow >= (int)tracks.size()) return;
    int trackId = tracks[targetRow].id;
    timeline->removeTrackAndPattern(trackId);
    if (auto* win = window()) win->redraw();
}

void TrackContextPopup::doShowParamSubmenu()
{
    if (!paramSubmenu) return;
    // Position the submenu to the right of (and aligned with) the Add parameter button.
    int btnY   = y() + 1 + 6*btnH;
    int subX   = x() + popW;
    int subY   = btnY;
    // Clamp so the submenu stays within the parent window.
    if (auto* win = window()) {
        int maxX = win->x() + win->w() - ParameterSubmenu::popW;
        int maxY = win->y() + win->h() - ParameterSubmenu::popH;
        if (subX > maxX) subX = x() - ParameterSubmenu::popW;
        if (subY > maxY) subY = maxY;
    }
    paramSubmenu->position(subX, subY);
    paramSubmenu->show();
}
