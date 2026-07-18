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
#include "toggleButton.hpp"
#include "modernChoice.hpp"
#include "modern/denomBeatChoice.hpp"
#include "modern/modernValueInput.hpp"
#include "panelStyle.hpp"
#include "chords.hpp"
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Control-row section structs
// Each inherits Fl_Flex and handles its own begin/end in its constructor so
// that member-initialisation order in PatternPanel naturally places them
// inside the outer controlRow flex group.
// ---------------------------------------------------------------------------

struct KeySection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 40;
    static constexpr int kBtnW    = 26;
    static constexpr int kChoiceW = 52;
    static constexpr int kOctW    = 46;
    static constexpr int kWidth   = kLabelW + kGap + kBtnW + kGap + kChoiceW + kGap + kOctW;
    Fl_Box       baseLabel;
    ToggleButton sharpFlatBtn;
    ModernChoice rootChoice;
    ModernChoice octaveChoice;   // octave offset: -1, 0, 1
    KeySection(int x, int y, int h);
};

struct ChordSection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 55;
    static constexpr int kChoiceW = 98;
    static constexpr int kWidth   = kLabelW + kGap + kChoiceW;
    ToggleButton chordScaleBtn;   // toggles the choice between chords and scales
    ModernChoice chordChoice;
    ChordSection(int x, int y, int h);
};

// Time signature. The denominator dropdown also carries the beat definition (a
// plain denominator counts in crotchets; the dotted variants show a note glyph).
struct TimeSigSection : Fl_Flex {
    static constexpr int kGap    = 3;
    static constexpr int kLabelW = 28;
    static constexpr int kNumW   = 26;
    static constexpr int kSlashW = 12;
    static constexpr int kDenW   = 54;   // hugs DenomBeatChoice::naturalWidth()
    static constexpr int kWidth  = kLabelW + kGap + kNumW + kGap + kSlashW + kGap + kDenW;
    Fl_Box           timeSigLabel;
    ModernValueInput timeSigNum;
    Fl_Box           timeSigSlash;
    DenomBeatChoice  timeSigDen;
    TimeSigSection(int x, int y, int h);
};

struct BarsSection : Fl_Flex {
    static constexpr int kGap    = 3;
    static constexpr int kLabelW = 36;
    static constexpr int kInputW = 26;
    static constexpr int kWidth  = kLabelW + kGap + kInputW;
    Fl_Box           barsLabel;
    ModernValueInput barsInput;
    BarsSection(int x, int y, int h);
};

// Beat subdivisions: the beat (one time-signature denominator unit) is split
// into 1, 2 or 3 parts. The Snap toggle decides whether edits quantise to them.
struct DivisionsSection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 24;
    static constexpr int kChoiceW = 62;
    static constexpr int kBtnW    = 44;
    static constexpr int kWidth   = kLabelW + kGap + kChoiceW + kGap + kBtnW;
    Fl_Box       divLabel;
    ModernChoice divChoice;
    ModernButton snapBtn;
    DivisionsSection(int x, int y, int h);
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
    static constexpr int      kWidth  = kMargin + TimeSigSection::kWidth + kGap + BarsSection::kWidth + kGap + DivisionsSection::kWidth + kMargin;
    static constexpr Fl_Color kBg     = 0x894B9400;
    TimeSigSection   timeSigSec;
    BarsSection      barsSec;
    DivisionsSection divSec;
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

class ObservableInstrument;

class PatternPanel : public Fl_Group, public ITimelineObserver {

    ObservablePattern*   pattern = nullptr;
    ObservableInstrument* instr_ = nullptr;
    int                 editingPatId = -1;
    std::string         originalLabel;
    bool                useSharp      = true;
    bool                showScale     = false;  // Chord/Scale toggle state

    InlineInput     input;           // direct child of PatternPanel for overlay
    Fl_Flex         controlRow;      // outer flex — all subsequent members go here
    RecenterButton  recentreBtn;
    ModernChoice    zoomChoice;      // sits just after recentreBtn; tooltip-only (no label)
    Fl_Box          patternName;
    ModernChoice    outChoice;
    HarmonyControls harmonyControls;
    TimeControls    timeControls;
    ModernButton    rapidBtn;        // immediately after timeControls
    Fl_Box          spacer;          // flexible gap fills remaining space

    float computeSnapBeats() const;
    int   computeDivisions() const;
    int   computeZoomFactor() const;

    void initControlRowLayout();
    void initPatternName();
    void initHarmonyControls();
    void initZoomChoice();
    void initTimeControls();
    void initOutChoice();
    void initRapidBtn();
    void initInput();

    void configureStandardRow();
    void configureDrumRow();
    void configurePianorollRow();

    void startEdit();
    void cancelEdit();
    void checkDuplicate();
    void updateRootChoiceLabels(int preserveIndex);
    void populateChordChoice();
    void refreshOutChoice();
    void refreshTimeSig();
    void refreshBars();
    void refreshHarmony();
    void refreshDivisions();
    void refreshZoom();
    void commitHarmony();
    int  selectedPatternId() const;

    void draw() override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

public:
    PatternPanel(int x, int y, int w, int h);
    ~PatternPanel();

    std::function<void()>      onParamsChanged;
    std::function<void()>      onFocus;
    std::function<void(float)> onSnapChanged;
    std::function<void(int)>   onDivisionsChanged;
    std::function<void(int)>   onZoomChanged;
    std::function<void(bool)>  onRapidChanged;

    void commitEdit();

    int  rootPitch() const { return harmonyControls.keySec.rootChoice.value(); }
    std::string chordHash() const {
        const Fl_Menu_Item* m = harmonyControls.chordSec.chordChoice.mvalue();
        int idx = m ? (int)(intptr_t)m->user_data() : 0;
        return chordDefs[idx].hash;
    }
    bool isSharp()   const { return useSharp; }
    int  octaveOffset() const { return harmonyControls.keySec.octaveChoice.value() - 1; }

    void setParams(int root, std::string_view chordHash, bool sharp, int octaveOffset);
    void setInstruments(ObservableInstrument* instr);

    void setPattern(ObservablePattern* tl);
    void onTimelineChanged() override;
};

#endif
