#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include "observableTimeline.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include "panelStyle.hpp"
#include <string>
#include <vector>

class PatternPanel : public Fl_Group, public ITimelineObserver {

    ObservableTimeline* timeline      = nullptr;
    int                 editingTrackId = -1;
    std::string         originalLabel;
    bool                useSharp      = true;
    std::vector<std::string> stdInstrumentNames_;
    std::vector<std::string> drumInstrumentNames_;

    Fl_Box         patternName;
    Fl_Box         baseLabel;
    ModernButton   sharpFlatBtn;
    ModernChoice   rootChoice;
    Fl_Box         chordLabel;
    ModernChoice   chordChoice;
    Fl_Box         timeSigLabel;
    Fl_Value_Input timeSigNum;
    Fl_Box         timeSigSlash;
    ModernChoice   timeSigDen;
    Fl_Box         barsLabel;
    Fl_Value_Input barsInput;
    ModernChoice   outChoice;
    InlineInput    input;

    void startEdit();
    void cancelEdit();
    void checkDuplicate();
    void updateRootChoiceLabels(int preserveIndex);
    void refreshOutChoice();
    void refreshTimeSig();
    void refreshBars();

    void draw() override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

public:
    PatternPanel(int x, int y, int w, int h);
    ~PatternPanel();

    std::function<void()> onParamsChanged;

    void commitEdit();

    int  rootPitch() const { return rootChoice.value(); }
    int  chordType() const { return chordChoice.value(); }
    bool isSharp()   const { return useSharp; }

    // Set chord/root/sharp without triggering onParamsChanged.
    void setParams(int root, int chord, bool sharp);

    // Update the available instrument lists shown in the Out dropdown.
    void setInstruments(const std::vector<std::string>& stdNames,
                        const std::vector<std::string>& drumNames);

    void setTimeline(ObservableTimeline* tl);
    void setDrumMode(bool drum);
    void onTimelineChanged() override;
};

#endif
