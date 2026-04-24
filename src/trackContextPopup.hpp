#ifndef TRACK_CONTEXT_POPUP_HPP
#define TRACK_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "observableTimeline.hpp"
#include <functional>

class TrackContextPopup : public BasePopup {
    static constexpr int popH  = 5 * 30 + 2;  // +2 for border

public:
    static constexpr int popW  = 150;
    static constexpr int btnH  = 30;

private:

    ModernButton*       addBtn;
    ModernButton*       addDrumBtn;
    ModernButton*       addPianorollBtn;
    ModernButton*       copyBtn;
    ModernButton*       deleteBtn;
    ObservableTimeline* timeline  = nullptr;
    int                 targetRow = -1;

    static constexpr int numPatternBeats = 8;

    void doAdd();
    void doAddDrum();
    void doAddPianoroll();
    void doCopy();
    void doDelete();

public:
    TrackContextPopup();

    void open(int row, ObservableTimeline* tl, int wx, int wy);
};

#endif
