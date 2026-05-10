#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include "observablePattern.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include "panelStyle.hpp"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Control-row section structs
// Each inherits Fl_Flex and handles its own begin/end in its constructor so
// that member-initialisation order in PatternPanel naturally places them
// inside the outer controlRow flex group.
// ---------------------------------------------------------------------------

struct KeySection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 55;
    static constexpr int kBtnW    = 26;
    static constexpr int kChoiceW = 110;
    static constexpr int kWidth   = kLabelW + kGap + kBtnW + kGap + kChoiceW;
    Fl_Box       baseLabel;
    ModernButton sharpFlatBtn;
    ModernChoice rootChoice;
    KeySection(int x, int y, int h);
};

struct ChordSection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 55;
    static constexpr int kChoiceW = 130;
    static constexpr int kWidth   = kLabelW + kGap + kChoiceW;
    Fl_Box       chordLabel;
    ModernChoice chordChoice;
    ChordSection(int x, int y, int h);
};

struct TimeSigSection : Fl_Flex {
    static constexpr int kGap    = 3;
    static constexpr int kLabelW = 28;
    static constexpr int kNumW   = 40;
    static constexpr int kSlashW = 12;
    static constexpr int kDenW   = 50;
    static constexpr int kWidth  = kLabelW + kGap + kNumW + kGap + kSlashW + kGap + kDenW;
    Fl_Box         timeSigLabel;
    Fl_Value_Input timeSigNum;
    Fl_Box         timeSigSlash;
    ModernChoice   timeSigDen;
    TimeSigSection(int x, int y, int h);
};

struct BarsSection : Fl_Flex {
    static constexpr int kGap    = 3;
    static constexpr int kLabelW = 36;
    static constexpr int kInputW = 40;
    static constexpr int kWidth  = kLabelW + kGap + kInputW;
    Fl_Box         barsLabel;
    Fl_Value_Input barsInput;
    BarsSection(int x, int y, int h);
};

struct SnapSection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 40;
    static constexpr int kChoiceW = 70;
    static constexpr int kWidth   = kLabelW + kGap + kChoiceW;
    Fl_Box       snapLabel;
    ModernChoice snapChoice;
    SnapSection(int x, int y, int h);
};

struct HarmonyControls : Fl_Flex {
    static constexpr int      kGap    = 3;
    static constexpr int      kMargin = 4;
    static constexpr int      kCtrlH  = 24;
    static constexpr int      kWidth  = kMargin + KeySection::kWidth + kGap + ChordSection::kWidth + kMargin;
    static constexpr Fl_Color kBg     = 0x7659B500;
    KeySection   keySec;
    ChordSection chordSec;
    HarmonyControls(int x, int y, int h);
    void draw() override;
    void resize(int x, int y, int w, int h) override;
};

struct TimeControls : Fl_Flex {
    static constexpr int      kGap    = 3;
    static constexpr int      kMargin = 4;
    static constexpr int      kCtrlH  = 24;
    static constexpr int      kWidth  = kMargin + TimeSigSection::kWidth + kGap + BarsSection::kWidth + kGap + SnapSection::kWidth + kMargin;
    static constexpr Fl_Color kBg     = 0x894B9400; 
    TimeSigSection timeSigSec;
    BarsSection    barsSec;
    SnapSection    snapSec;
    TimeControls(int x, int y, int h);
    void draw() override;
    void resize(int x, int y, int w, int h) override;
};

// ---------------------------------------------------------------------------

class RecenterButton : public Fl_Widget {
    bool hovered = false;

    void draw() override {
        Fl_Color bg = hovered ? panelBgHover : panelBg;
        fl_color(bg);
        fl_rectf(x(), y(), w(), h());
        fl_color(panelCtrlBorder);
        fl_rect(x(), y(), w(), h());
        int cx = x() + w() / 2;
        int cy = y() + h() / 2;
        int r  = std::min(w(), h()) / 4;
        int tk = 3;
        fl_color(0x94A3B800);
        fl_line_style(FL_SOLID, 1);
        fl_arc(cx - r, cy - r, 2 * r, 2 * r, 0, 360);
        fl_line(cx,         cy - r - 1,     cx,         cy - r - 1 - tk);
        fl_line(cx,         cy + r + 1,     cx,         cy + r + 1 + tk);
        fl_line(cx - r - 1, cy,             cx - r - 1 - tk, cy);
        fl_line(cx + r + 1, cy,             cx + r + 1 + tk, cy);
        fl_line_style(0);
    }

    int handle(int event) override {
        switch (event) {
        case FL_ENTER: hovered = true;  redraw(); return 1;
        case FL_LEAVE: hovered = false; redraw(); return 1;
        case FL_PUSH:    return 1;
        case FL_RELEASE:
            if (Fl::event_inside(this)) do_callback();
            return 1;
        default: return Fl_Widget::handle(event);
        }
    }

public:
    RecenterButton(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}
};

class PatternPanel : public Fl_Group, public ITimelineObserver {

    ObservablePattern* pattern      = nullptr;
    int                 editingTrackId = -1;
    std::string         originalLabel;
    bool                useSharp      = true;
    std::vector<std::string> stdInstrumentNames_;
    std::vector<std::string> drumInstrumentNames_;

    InlineInput     input;           // direct child of PatternPanel for overlay
    Fl_Flex         controlRow;      // outer flex — all subsequent members go here
    RecenterButton  recentreBtn;
    Fl_Box          patternName;
    ModernChoice    outChoice;
    HarmonyControls harmonyControls;
    TimeControls    timeControls;

    float computeSnapBeats() const;

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

    std::function<void()>      onParamsChanged;
    std::function<void()>      onFocus;
    std::function<void(float)> onSnapChanged;

    void commitEdit();

    int  rootPitch() const { return harmonyControls.keySec.rootChoice.value(); }
    int  chordType() const { return harmonyControls.chordSec.chordChoice.value(); }
    bool isSharp()   const { return useSharp; }

    // Set chord/root/sharp without triggering onParamsChanged.
    void setParams(int root, int chord, bool sharp);

    // Update the available instrument lists shown in the Out dropdown.
    void setInstruments(const std::vector<std::string>& stdNames,
                        const std::vector<std::string>& drumNames);

    void setPattern(ObservablePattern* tl);
    void setDrumMode(bool drum);
    void onTimelineChanged() override;
};

#endif
