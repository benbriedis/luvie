// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SELECTION_CONTEXT_POPUP_HPP
#define SELECTION_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include <functional>

class Grid;

// Shown instead of PatternInstanceContextPopup when the right-click lands on a
// block that is part of a multi-selection. Its items act on the whole selection,
// so it is a separate menu rather than a mode of the single-instance one.
class SelectionContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 170;

    SelectionContextPopup();

    void open(Grid* grid, std::function<void()> onCopy, std::function<void()> onDelete);

private:
    std::function<void()> onCopyFn;
    std::function<void()> onDeleteFn;
};

#endif
