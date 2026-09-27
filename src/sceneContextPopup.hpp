// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SCENE_CONTEXT_POPUP_HPP
#define SCENE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "sceneTriggers.hpp"
#include <FL/Fl_Box.H>
#include <array>

// Context menu shown when right-clicking a scene button in the Loop Editor: MIDI
// learn for the button, so a controller can switch scenes. Below a divider are the
// Next, Previous and First scene triggers. Those are shared by every scene and have
// no button of their own, so every scene button's menu offers them.
class SceneContextPopup : public ContextMenuPopup {
    static constexpr int kNav     = SceneTriggerMap::kSlots - SceneTriggerMap::kNextScene;
    static constexpr int dividerH = 9;

    // One learn item and its Clear per slot this menu edits: the scene's own first,
    // then the navigation slots in order.
    struct Row {
        int           slot     = -1;
        ModernButton* learnBtn = nullptr;
        ModernButton* clearBtn = nullptr;
    };
    std::array<Row, 1 + kNav> rows;
    Fl_Box* divider = nullptr;

    static void learnCb(Fl_Widget* w, void* d) {
        auto* self = static_cast<SceneContextPopup*>(d);
        self->hide();
        if (!self->triggers) return;
        for (const Row& r : self->rows)
            if (r.learnBtn == w) self->triggers->toggleLearn(r.slot);
    }
    static void clearCb(Fl_Widget* w, void* d) {
        auto* self = static_cast<SceneContextPopup*>(d);
        self->hide();
        if (!self->triggers) return;
        for (const Row& r : self->rows)
            if (r.clearBtn == w) self->triggers->clear(r.slot);
    }

public:
    static constexpr int popW = 290;

    // Where the items act. Without it the menu never opens.
    SceneTriggerMap* triggers = nullptr;

    SceneContextPopup() : ContextMenuPopup(popW, 2*30+2) {
        for (int i = 0; i < (int)rows.size(); i++) {
            rows[i].slot     = i == 0 ? -1 : SceneTriggerMap::kNextScene + i - 1;
            rows[i].learnBtn = addItem(0, "MIDI learn");
            rows[i].clearBtn = addItem(0, "Clear MIDI learn");
            rows[i].learnBtn->callback(learnCb, this);
            rows[i].clearBtn->callback(clearCb, this);
        }
        for (int i = 1; i < (int)rows.size(); i++)
            rows[i].clearBtn->copy_label(
                ("Clear " + SceneTriggerMap::slotName(rows[i].slot)).c_str());
        divider = new Fl_Box(1, 0, popW - 2, 1);
        divider->box(FL_FLAT_BOX);
        divider->color(0xCBD5E100);

        end();
        hide();
    }

    void open(int scene, int wx, int wy) {
        if (!triggers) return;
        rows[0].slot = scene;

        // Each Clear only when there is a trigger to clear, so the rows are stacked
        // here, and popH follows, because ContextMenuPopup::resize() snaps the window
        // back to popH.
        int y = 1;
        auto place = [&](ModernButton* b, bool vis) {
            if (!vis) { b->hide(); return; }
            b->resize(b->x(), y, b->w(), b->h());
            b->show();
            y += btnH;
        };
        for (int i = 0; i < (int)rows.size(); i++) {
            if (i == 1) {
                divider->resize(divider->x(), y + dividerH / 2, divider->w(), 1);
                y += dividerH;
            }
            const Row& r = rows[i];
            r.learnBtn->copy_label(triggers->learnMenuLabel(r.slot).c_str());
            place(r.learnBtn, true);
            place(r.clearBtn, triggers->bindingFor(r.slot) != nullptr);
        }
        popH = y + 1;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
