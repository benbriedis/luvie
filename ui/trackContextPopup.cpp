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

    addBtn    = makeItem(1,           popW, "Add Pattern");
    copyBtn   = makeItem(1 + btnH,    popW, "Copy Pattern");
    deleteBtn = makeItem(1 + 2*btnH,  popW, "Delete Pattern");

    addBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doAdd();
    }, this);
    copyBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doCopy();
    }, this);
    deleteBtn->callback([](Fl_Widget*, void* d) {
        static_cast<TrackContextPopup*>(d)->doDelete();
    }, this);

    end();
    hide();
}

void TrackContextPopup::open(int row, ObservableTimeline* tl, int wx, int wy)
{
    timeline  = tl;
    targetRow = row;

    position(wx, wy);
    if (auto* aw = dynamic_cast<AppWindow*>(window()))
        aw->openPopup(this);
    else
        show();
    redraw();
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
