#ifndef LOOP_RULER_CONTEXT_POPUP_HPP
#define LOOP_RULER_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include <functional>

// Context menu shown when right-clicking the loop ruler. The two items place the
// Start or End marker at the right-clicked bar; the ruler supplies the handlers
// at open() time (they capture the clicked bar).
class LoopRulerContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 120;

    LoopRulerContextPopup();

    void open(int wx, int wy, std::function<void()> onSetStart,
              std::function<void()> onSetEnd);

private:
    ModernButton*         setStartBtn;
    ModernButton*         setEndBtn;
    std::function<void()> setStartFn;
    std::function<void()> setEndFn;
};

#endif
