// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include "observablePattern.hpp"
#include "inlineInput.hpp"
#include "controlBar.hpp"
#include "timeSigSection.hpp"   // TimeSigSection: shared with the Loop Editor's bar
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
#include "modern/accidentalChoice.hpp"
#include "modern/denomBeatChoice.hpp"
#include "modern/sharpFlatButton.hpp"
#include "modern/recordButton.hpp"
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
    static constexpr int kWidth   = kLabelW + kGap + kBtnW + kGap + kChoiceW;
    Fl_Box           baseLabel;
    SharpFlatButton  sharpFlatBtn;
    AccidentalChoice rootChoice;
    KeySection(int x, int y, int h);
};

struct ChordSection : Fl_Flex {
    static constexpr int kGap     = 3;
    static constexpr int kLabelW  = 55;
    static constexpr int kChoiceW = 98;
    static constexpr int kWidth   = kLabelW + kGap + kChoiceW;
    ToggleButton     chordScaleBtn;   // toggles the choice between chords and scales
    AccidentalChoice chordChoice;     // names carry accidentals: "7(b9)", "maj7(#11)"
    ChordSection(int x, int y, int h);
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

class PatternPanel : public ControlBar, public ITimelineObserver {

    ObservablePattern*   pattern = nullptr;
    ObservableInstrument* instr_ = nullptr;
    int                 editingPatId = -1;
    std::string         originalLabel;
    bool                useSharp      = true;
    bool                showScale     = false;  // Chord/Scale toggle state
    // Only the harmony bar folds; the drum and pianoroll bars carry less and
    // stay on one row whatever the width.
    bool                canFold       = true;
    // The instrument whose notes set the base note, or -1. Shown in the base note's
    // tooltip; the routing itself is the owner's.
    int                 rootTriggerId_ = -1;

    InlineInput     input;           // direct child of PatternPanel for overlay
    RecenterButton  recentreBtn;
    ModernChoice    zoomChoice;      // sits just after recentreBtn; tooltip-only (no label)
    Fl_Box          patternName;
    ModernChoice    outChoice;
    HarmonyControls harmonyControls;
    TimeControls    timeControls;
    ModernButton    rapidBtn;
    // Flexible bars, immediately left of Record. Off, a take records into a pattern
    // of the length it already has; on, the pattern gains bars as the take runs.
    // Both toggles show the state of the pattern on screen: each pattern is armed on
    // its own and stays armed while others are shown (see PatternRecorders). Session
    // state only — deliberately not stored in the project.
    ModernButton    growBtn;
    // Record arm, on the right of the bar. Pianoroll and drum only — the harmony
    // editor cannot record, so it hides this like it does Rapid.
    RecordButton    recordBtn;

    float computeSnapBeats() const;
    int   computeDivisions() const;
    float computeZoomFactor() const;
    int   selectedZoomPct() const;

    std::vector<PanelRow> buildLayout(int availW) override;
    void                  afterLayout() override;

    void initControls();
    void initPatternName();
    void initHarmonyControls();
    void initZoomChoice();
    void initTimeControls();
    void initOutChoice();
    void initRapidBtn();
    void initRecordBtn();
    void initGrowBtn();
    // Sets a toggle and its colours without firing its callback.
    void showRecord(bool on);
    void showGrow(bool on);
    void initInput();

    void configureHarmonyRow();
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
    void refreshRootTooltip();
    void refreshDivisions();
    void refreshZoom();
    void commitHarmony();
    int  selectedPatternId() const;

    int  handle(int event) override;

public:
    PatternPanel(int x, int y, int w, int h);
    ~PatternPanel();

    std::function<void()>      onParamsChanged;
    std::function<void()>      onFocus;
    std::function<void(float)> onSnapChanged;
    std::function<void(int)>   onDivisionsChanged;
    std::function<void(float)> onZoomChanged;
    std::function<void(bool)>  onRapidChanged;
    // Record arm/disarm from the toggle, for the pattern on screen.
    std::function<void(bool)>  onRecordChanged;
    // Flexible bars on/off from the toggle, likewise.
    std::function<void(bool)>  onGrowChanged;
    // Right-click on the base note, at window coordinates: the owner opens the
    // menu that picks the base note's MIDI trigger instrument.
    std::function<void(int wx, int wy)> onRootContextMenu;

    // The instrument whose notes set the base note, or -1 for none.
    void setRootTrigger(int instrId);
    // A note from that instrument: the selected pattern, if it is a harmony
    // pattern, takes its pitch class as the base note. The octave is dropped — the
    // base note has none.
    void setRootFromMidi(int midiNote);

    void commitEdit();

    int  rootPitch() const { return harmonyControls.keySec.rootChoice.value(); }
    std::string chordHash() const {
        const Fl_Menu_Item* m = harmonyControls.chordSec.chordChoice.mvalue();
        int idx = m ? (int)(intptr_t)m->user_data() : 0;
        return chordDefs[idx].hash;
    }
    bool isSharp()   const { return useSharp; }

    // Show the arm states of the pattern on screen. The owner keeps them, per
    // pattern, and calls this whenever the pattern shown or its states change.
    void showArmState(bool record, bool grow) { showRecord(record); showGrow(grow); }

    void setParams(int root, std::string_view chordHash, bool sharp);
    void setInstruments(ObservableInstrument* instr);

    void setPattern(ObservablePattern* tl);
    void onTimelineChanged() override;
};

#endif
