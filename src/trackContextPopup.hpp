#ifndef TRACK_CONTEXT_POPUP_HPP
#define TRACK_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "modernButton.hpp"
#include "observableTimeline.hpp"
#include "parameterSubmenu.hpp"
#include <functional>

class TrackContextPopup : public BasePopup {
    static constexpr int popH  = 7 * 30 + 2;  // +2 for border

public:
    static constexpr int popW  = 150;
    static constexpr int btnH  = 30;

private:

    ModernButton*       openPatternBtn;
    ModernButton*       addBtn;
    ModernButton*       addDrumBtn;
    ModernButton*       addPianorollBtn;
    ModernButton*       copyBtn;
    ModernButton*       deleteBtn;
    ModernButton*       addParamBtn;
    ObservableTimeline* timeline  = nullptr;
    int                 targetRow = -1;

    static constexpr int numPatternBeats = 8;

    void doOpenPattern();
    void doAdd();
    void doAddDrum();
    void doAddPianoroll();
    void doCopy();
    void doDelete();
    void doShowParamSubmenu();

public:
    ParameterSubmenu*                    paramSubmenu = nullptr;

    TrackContextPopup();

    std::function<void(int trackIndex)>  onOpenPattern;
    std::function<void(const char*)>     onAddParameter;

    void open(int row, ObservableTimeline* tl, int wx, int wy);
};

#endif
