#ifndef MARKER_CONTEXT_POPUP_HPP
#define MARKER_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include <functional>

// Right-click menu on a song-editor ruler; offers to add a marker of the
// ruler's own kind (BPM or time signature) at the clicked bar.
class MarkerContextPopup : public ContextMenuPopup {
public:
    static constexpr int popW = 170;

    MarkerContextPopup();

    void open(const char* label, std::function<void()> onAdd, int wx, int wy);

private:
    ModernButton*         addBtn;
    std::function<void()> onAddCb;
};

#endif
