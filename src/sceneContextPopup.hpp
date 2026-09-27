// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SCENE_CONTEXT_POPUP_HPP
#define SCENE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "sceneTriggers.hpp"

// Context menu shown when right-clicking a scene button in the Loop Editor: MIDI
// learn for the button, so a controller can switch scenes.
class SceneContextPopup : public ContextMenuPopup {
    int           scene         = -1;
    ModernButton* learnBtn      = nullptr;
    ModernButton* clearLearnBtn = nullptr;

public:
    static constexpr int popW = 200;

    // Where "MIDI learn" and "Clear MIDI learn" act. Without it the menu never opens.
    SceneTriggerMap* triggers = nullptr;

    SceneContextPopup() : ContextMenuPopup(popW, 2*30+2) {
        learnBtn      = addItem(0, "MIDI learn");
        clearLearnBtn = addItem(1, "Clear MIDI learn");

        learnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<SceneContextPopup*>(d);
            self->hide();
            if (self->triggers) self->triggers->toggleLearn(self->scene);
        }, this);
        clearLearnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<SceneContextPopup*>(d);
            self->hide();
            if (self->triggers) self->triggers->clear(self->scene);
        }, this);

        end();
        hide();
    }

    void open(int scene_, int wx, int wy) {
        if (!triggers) return;
        scene = scene_;
        learnBtn->copy_label(triggers->learnMenuLabel(scene).c_str());
        // "Clear MIDI learn" only when there is a trigger to clear. popH follows,
        // because ContextMenuPopup::resize() snaps the window back to popH.
        int shown = 1;
        if (triggers->bindingFor(scene)) { clearLearnBtn->show(); shown++; }
        else                              clearLearnBtn->hide();
        popH = shown * btnH + 2;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
