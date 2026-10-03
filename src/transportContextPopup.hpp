// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef TRANSPORT_CONTEXT_POPUP_HPP
#define TRANSPORT_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "sceneTriggers.hpp"

// Context menu shown when right-clicking the transport's Play/Pause or Rewind
// button: MIDI learn for the button, so a controller can press it.
class TransportContextPopup : public ContextMenuPopup {
    ModernButton* learnBtn = nullptr;
    ModernButton* clearBtn = nullptr;
    int           slot     = -1;

public:
    static constexpr int popW = 220;

    // Where the items act. Without it the menu never opens.
    SceneTriggerMap* triggers = nullptr;

    TransportContextPopup() : ContextMenuPopup(popW, 2*30+2) {
        learnBtn = addItem(0, "MIDI learn");
        clearBtn = addItem(1, "Clear MIDI learn");
        learnBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<TransportContextPopup*>(d);
            self->hide();
            if (self->triggers) self->triggers->toggleLearn(self->slot);
        }, this);
        clearBtn->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<TransportContextPopup*>(d);
            self->hide();
            if (self->triggers) self->triggers->clear(self->slot);
        }, this);
        end();
        hide();
    }

    // `s` is SceneTriggerMap::kPlayPause or kRewind.
    void open(int s, int wx, int wy) {
        if (!triggers) return;
        slot = s;
        // Clear only when there is a trigger to clear; popH follows, because
        // ContextMenuPopup::resize() snaps the window back to popH.
        learnBtn->copy_label(triggers->learnMenuLabel(slot).c_str());
        const bool bound = triggers->bindingFor(slot) != nullptr;
        bound ? clearBtn->show() : clearBtn->hide();
        popH = (bound ? 2 : 1) * btnH + 2;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
