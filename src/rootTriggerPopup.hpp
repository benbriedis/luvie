// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef ROOT_TRIGGER_POPUP_HPP
#define ROOT_TRIGGER_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "timeline.hpp"
#include <FL/Fl_Box.H>
#include <functional>
#include <string>
#include <vector>

// Context menu shown when right-clicking the Harmony Editor's base note: picks the
// instrument whose notes set it. Rather than learning one key, the base note
// follows whatever that instrument plays — its input, channel and split decide
// which keys count — so a split keyboard's lower half can change key while the
// upper half plays.
class RootTriggerPopup : public ContextMenuPopup {
    static constexpr int headingH = 26;

    Fl_Box* heading = nullptr;
    // "None", then one item per instrument, rebuilt on every open.
    std::vector<ModernButton*> items;
    std::vector<int>           itemIds;   // the instrument each item picks; -1 for None

    static void itemCb(Fl_Widget* w, void* d) {
        auto* self = static_cast<RootTriggerPopup*>(d);
        self->hide();
        for (int i = 0; i < (int)self->items.size(); i++)
            if (self->items[i] == w && self->onSelect) self->onSelect(self->itemIds[i]);
    }

public:
    static constexpr int popW = 240;

    // An item was chosen: the instrument id, or -1 for none.
    std::function<void(int instrId)> onSelect;

    RootTriggerPopup() : ContextMenuPopup(popW, headingH + btnH + 2) {
        heading = new Fl_Box(1, 1, popW - 2, headingH, "Set base note from instrument");
        heading->box(FL_NO_BOX);
        heading->labelsize(11);
        heading->labelcolor(0x6B728000);
        heading->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        end();
        hide();
    }

    // Lists `instruments`, ticking `current` (None when it is not among them).
    void open(const std::vector<Instrument>& instruments, int current, int wx, int wy) {
        for (ModernButton* b : items) {
            remove(b);
            Fl::delete_widget(b);
        }
        items.clear();
        itemIds.clear();

        bool found = false;
        for (const auto& in : instruments) found = found || in.id == current;

        begin();
        auto add = [&](const std::string& label, int id, bool ticked) {
            ModernButton* b = addItem(0, "");
            b->copy_label(label.c_str());
            b->position(1, 1 + headingH + (int)items.size() * btnH);
            b->reserveTick(true);
            b->setTicked(ticked);
            b->callback(itemCb, this);
            items.push_back(b);
            itemIds.push_back(id);
        };
        add("None", -1, !found);
        for (const auto& in : instruments)
            add(in.name, in.id, in.id == current);
        end();

        popH = 1 + headingH + (int)items.size() * btnH + 1;
        size(popW, popH);
        openAt(wx, wy);
    }
};

#endif
