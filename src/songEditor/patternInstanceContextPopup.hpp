#ifndef PATTERN_INSTANCE_CONTEXT_POPUP_HPP
#define PATTERN_INSTANCE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "timeline.hpp"
#include <functional>
#include <vector>

class Grid;

class PatternInstanceContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 150;

    PatternInstanceContextPopup();

    void open(std::vector<Note>* notes, int noteIdx, Grid* grid,
              std::function<void()> onDelete,
              std::function<void()> onOpenPattern);

private:
    std::function<void()> onDeleteFn;
    std::function<void()> onOpenPatternFn;
};

#endif
