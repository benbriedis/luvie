#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include "observableTimeline.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <string>

class PatternPanel : public Fl_Group, public ITimelineObserver {
    static constexpr Fl_Color bg     = 0x1E293B00;
    static constexpr Fl_Color border = 0x37415100;
    static constexpr Fl_Color text   = FL_WHITE;

    ObservableTimeline* timeline      = nullptr;
    int                 editingTrackId = -1;
    std::string         originalLabel;
    bool                useSharp      = true;

    Fl_Box      patternName;
    Fl_Box      baseLabel;
    Fl_Button   sharpFlatBtn;
    Fl_Choice   rootChoice;
    Fl_Choice   octaveChoice;
    Fl_Box      chordLabel;
    Fl_Choice   chordChoice;
    InlineInput input;

    void startEdit();
    void cancelEdit();
    void checkDuplicate();
    void updateRootChoiceLabels(int preserveIndex);

    void draw() override;
    int  handle(int event) override;

public:
    PatternPanel(int x, int y, int w, int h);
    ~PatternPanel();

    std::function<void()> onParamsChanged;

    void commitEdit();

    int  octave()    const { return octaveChoice.value(); }
    int  rootPitch() const { return rootChoice.value(); }
    int  chordType() const { return chordChoice.value(); }
    bool isSharp()   const { return useSharp; }

    void setTimeline(ObservableTimeline* tl);
    void onTimelineChanged() override;
};

#endif
