// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PASTE_CONTEXT_POPUP_HPP
#define PASTE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include <functional>

class Fl_Widget;

// Shown when a right-click lands on empty grid — no item under the cursor to
// have a menu of its own — and the clipboard holds something this grid could
// take. It is the mouse's way to the same thing ctrl-V does, and it pastes where
// the right-click was, not where the click on the menu item lands.
//
// Deliberately NOT paired with the context-menu cursor the way the item menus
// are. Empty grid is most of the grid, so hovering it would put that cursor up
// almost permanently and it would stop meaning "there is a menu on this thing".
class PasteContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 170;

    PasteContextPopup();

    void open(Fl_Widget* owner, std::function<void()> onPaste);

private:
    std::function<void()> onPasteFn;
};

#endif
