// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "loopContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

LoopContextPopup::LoopContextPopup()
    : ContextMenuPopup(popW, 8*30+2)
{
    openPatternBtn      = addItem(0, "Open pattern");
    addLaneBtn          = addItem(1, "Add harmony pattern");
    addPianorollLaneBtn = addItem(2, "Add pianoroll pattern");
    cloneLaneBtn        = addItem(3, "Clone pattern");
    removeLaneBtn       = addItem(4, "Remove pattern");
    showInstrumentsBtn  = addItem(5, "Show instruments");
    learnBtn            = addItem(6, "MIDI learn");
    clearLearnBtn       = addItem(7, "Clear MIDI learn");

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
    learnBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doLearn();
    }, this);
    clearLearnBtn->callback([](Fl_Widget*, void* d) {
        static_cast<LoopContextPopup*>(d)->doClearLearn();
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
    targetPatId   = -1;
    if (tl && !fromLabel)
        for (const auto& t : tl->song()->get().tracks)
            if (t.id == trackId)
                for (const auto& l : t.lanes)
                    if (l.id == laneId) targetPatId = l.patternId;

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
    // Add Pattern copies the track's existing pattern type, so name it for what
    // it will actually create.
    addLaneBtn->label(flags.isDrumTrack ? "Add drum pattern" : "Add harmony pattern");

    // MIDI learn needs a specific pattern, like Open; Clear shows only when there is
    // a trigger to clear. The rows are stacked here, and popH follows, because
    // ContextMenuPopup::resize() snaps the window back to popH.
    int y = 1 + 6 * btnH;
    auto place = [&](ModernButton* b, bool vis) {
        if (!vis) { b->hide(); return; }
        b->resize(b->x(), y, b->w(), b->h());
        b->show();
        y += btnH;
    };
    const bool canLearn = triggers && targetPatId >= 0;
    learnBtn->copy_label(canLearn ? triggers->patternLearnMenuLabel(targetPatId).c_str()
                                  : "MIDI learn");
    canLearn ? learnBtn->activate() : learnBtn->deactivate();
    place(learnBtn,      triggers != nullptr);
    place(clearLearnBtn, canLearn && triggers->patternBindingFor(targetPatId));
    popH = y + 1;
    size(popW, popH);

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

void LoopContextPopup::doLearn()
{
    hide();
    if (triggers && targetPatId >= 0) triggers->toggleLearnPattern(targetPatId);
}

void LoopContextPopup::doClearLearn()
{
    hide();
    if (triggers && targetPatId >= 0) triggers->clearPattern(targetPatId);
}
