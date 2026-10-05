// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SELECTION_CONTEXT_POPUP_HPP
#define SELECTION_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include <functional>

class Fl_Widget;

// Shown instead of the menu for a single item when the right-click lands on one
// that is part of a multi-selection. Its items act on the whole selection, so it
// is a separate menu rather than a mode of the single-item one. Shared by every
// editing grid: song blocks, pattern notes and drum hits all offer the same
// three. A pattern editor's time slice adds a fourth, "Delete slice", by passing
// onDeleteSlice; without it the item is not shown.
class SelectionContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 170;

    SelectionContextPopup();

    void open(Fl_Widget* owner, std::function<void()> onCut,
                                std::function<void()> onCopy,
                                std::function<void()> onDelete,
                                std::function<void()> onDeleteSlice = {});

private:
    ModernButton*         delSliceBtn = nullptr;
    std::function<void()> onCutFn;
    std::function<void()> onCopyFn;
    std::function<void()> onDeleteFn;
    std::function<void()> onDeleteSliceFn;
};

#endif
