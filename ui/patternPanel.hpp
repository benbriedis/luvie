#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include "observableTimeline.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <string>

class PatternPanel : public Fl_Group, public ITimelineObserver {
    static constexpr Fl_Color bg     = 0x1E293B00;
    static constexpr Fl_Color border = 0x37415100;
    static constexpr Fl_Color text   = FL_WHITE;

    ObservableTimeline* timeline      = nullptr;
    int                 editingTrackId = -1;
    std::string         originalLabel;

    Fl_Box      patternName;
    Fl_Box      octaveLabel;
    Fl_Choice   octaveChoice;
    Fl_Box      rootLabel;
    Fl_Choice   rootChoice;
    Fl_Box      chordLabel;
    Fl_Choice   chordChoice;
    InlineInput input;

    void startEdit();
    void cancelEdit();
    void checkDuplicate();

    void draw() override;
    int  handle(int event) override;

public:
    PatternPanel(int x, int y, int w, int h);
    ~PatternPanel();

    void commitEdit();

    void setTimeline(ObservableTimeline* tl);
    void onTimelineChanged() override;
};

#endif
