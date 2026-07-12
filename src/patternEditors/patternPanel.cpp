#include "patternPanel.hpp"
#include "observableInstrument.hpp"
#include "chords.hpp"
#include "timeSettings.hpp"
#include "panelStyle.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cmath>

static constexpr int pad           = 3;
static constexpr int sg            = 3;
static constexpr int groupGap      = 12;
static constexpr int ctrlH         = 24;
static constexpr int labelW        = 55;
static constexpr int nameW         = 150;
static constexpr int recentreBtnW  = 26;
static constexpr int toggleBtnW    = 26;
static constexpr int rootChoiceW   = 110;
static constexpr int choiceW       = 130;
static constexpr int timeSigLabelW = 28;
static constexpr int timeSigNumW   = 40;
static constexpr int slashW        = 12;
static constexpr int timeSigDenW   = 50;
static constexpr int barsLabelW    = 36;
static constexpr int barsInputW    = 40;
static constexpr int outChoiceW    = 155;
static constexpr int rapidBtnW     = 52;
static constexpr Fl_Color kRapidActiveColor = 0x3B82F600;
static constexpr Fl_Color kSnapActiveColor  = 0x3B82F600;

// Divisions split one beat (a time-signature denominator unit) into this many
// parts; the choice labels are "None" (1), "1/2", "1/3", "1/5" and "1/7".
static constexpr int kDivisors[]       = { 1, 2, 3, 5, 7 };
static constexpr int kDivisionsDefault = 0;  // None
static constexpr int kZoomFactors[]    = { 1, 2, 4 };
static constexpr int kZoomDefault      = 1;  // x2

// ---------------------------------------------------------------------------
// Section struct constructors
// ---------------------------------------------------------------------------

KeySection::KeySection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      baseLabel   (0, 0, kLabelW,  h, "Base"),
      sharpFlatBtn(0, 0, kBtnW,    h, "#", "b"),
      rootChoice  (0, 0, kChoiceW, h),
      octaveChoice(0, 0, kOctW,    h)
{
    gap(kGap);
    fixed(&baseLabel,    kLabelW);
    fixed(&sharpFlatBtn, kBtnW);
    fixed(&rootChoice,   kChoiceW);
    fixed(&octaveChoice, kOctW);
    end();
}

ChordSection::ChordSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      chordScaleBtn(0, 0, kLabelW,  h, "Chord", "Scale"),
      chordChoice  (0, 0, kChoiceW, h)
{
    gap(kGap);
    fixed(&chordScaleBtn, kLabelW);
    fixed(&chordChoice,   kChoiceW);
    end();
}

TimeSigSection::TimeSigSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      timeSigLabel(0, 0, kLabelW, h, "Sig"),
      timeSigNum  (0, 0, kNumW,   h),
      timeSigSlash(0, 0, kSlashW, h, "/"),
      timeSigDen  (0, 0, kDenW,   h),
      beatChoice  (0, 0, kBeatW,  h)
{
    gap(kGap);
    fixed(&timeSigLabel, kLabelW);
    fixed(&timeSigNum,   kNumW);
    fixed(&timeSigSlash, kSlashW);
    fixed(&timeSigDen,   kDenW);
    fixed(&beatChoice,   kBeatW);
    end();
}

BarsSection::BarsSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      barsLabel(0, 0, kLabelW, h, "Bars"),
      barsInput(0, 0, kInputW, h)
{
    gap(kGap);
    fixed(&barsLabel, kLabelW);
    fixed(&barsInput, kInputW);
    end();
}

ZoomSection::ZoomSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      zoomLabel (0, 0, kLabelW,  h, "Zoom"),
      zoomChoice(0, 0, kChoiceW, h)
{
    gap(kGap);
    fixed(&zoomLabel,  kLabelW);
    fixed(&zoomChoice, kChoiceW);
    end();
}

DivisionsSection::DivisionsSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      divLabel (0, 0, kLabelW,  h, "Div"),
      divChoice(0, 0, kChoiceW, h),
      snapBtn  (0, 0, kBtnW,    h, "Snap")
{
    gap(kGap);
    fixed(&divLabel,  kLabelW);
    fixed(&divChoice, kChoiceW);
    fixed(&snapBtn,   kBtnW);
    end();
}

HarmonyControls::HarmonyControls(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      keySec  (0, 0, h),
      chordSec(0, 0, h)
{
    gap(kGap);
    margin(kMargin, 0, kMargin, 0);
    fixed(&keySec,   KeySection::kWidth);
    fixed(&chordSec, ChordSection::kWidth);
    end();
}

void HarmonyControls::draw()
{
    fl_color(kBg);
    fl_rectf(x(), y(), w(), h());
    draw_children();
}

void HarmonyControls::resize(int x, int y, int w, int h)
{
    int vpad = (h - kCtrlH) / 2;
    margin(kMargin, vpad, kMargin, vpad);
    Fl_Flex::resize(x, y, w, h);
}

TimeControls::TimeControls(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      timeSigSec(0, 0, h),
      barsSec   (0, 0, h),
      zoomSec   (0, 0, h),
      divSec    (0, 0, h)
{
    gap(kGap);
    margin(kMargin, 0, kMargin, 0);
    fixed(&timeSigSec, TimeSigSection::kWidth);
    fixed(&barsSec,    BarsSection::kWidth);
    fixed(&zoomSec,    ZoomSection::kWidth);
    fixed(&divSec,     DivisionsSection::kWidth);
    end();
}

void TimeControls::draw()
{
    fl_color(kBg);
    fl_rectf(x(), y(), w(), h());
    draw_children();
}

void TimeControls::resize(int x, int y, int w, int h)
{
    int vpad = (h - kCtrlH) / 2;
    margin(kMargin, vpad, kMargin, vpad);
    Fl_Flex::resize(x, y, w, h);
}

// ---------------------------------------------------------------------------

int PatternPanel::selectedPatternId() const
{
    if (!pattern) return 0;
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) return 0;
    return tl.patternIdForSelectedLane();
}

// The snap quantum in grid beats (one beat = one time-signature denominator
// unit), or 0 when snapping is off. Divisions are fractions of that beat, so
// the time signature itself does not enter into it.
float PatternPanel::computeSnapBeats() const
{
    if (!timeControls.divSec.snapBtn.value()) return 0.0f;
    int idx = timeControls.divSec.divChoice.value();
    if (idx < 0 || idx >= (int)std::size(kDivisors)) idx = kDivisionsDefault;
    return 1.0f / (float)kDivisors[idx];
}

// How many parts the beat is split into (1 = None).
int PatternPanel::computeDivisions() const
{
    int idx = timeControls.divSec.divChoice.value();
    if (idx < 0 || idx >= (int)std::size(kDivisors)) idx = kDivisionsDefault;
    return kDivisors[idx];
}

int PatternPanel::computeZoomFactor() const
{
    int idx = timeControls.zoomSec.zoomChoice.value();
    if (idx < 0 || idx >= (int)std::size(kZoomFactors)) idx = kZoomDefault;
    return kZoomFactors[idx];
}

PatternPanel::PatternPanel(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      input          (0, 0, nameW,       ctrlH),
      controlRow     (x, y, w, h,        Fl_Flex::HORIZONTAL),
      recentreBtn    (0, 0, recentreBtnW,ctrlH),
      patternName    (0, 0, nameW,       ctrlH),
      outChoice      (0, 0, outChoiceW,  ctrlH),
      harmonyControls(0, 0, ctrlH),
      timeControls   (0, 0, ctrlH),
      rapidBtn       (0, 0, rapidBtnW,   ctrlH, "Rapid"),
      spacer         (0, 0, 0,           0)
{
    initControlRowLayout();
    initPatternName();
    initHarmonyControls();
    initTimeControls();
    initOutChoice();
    initRapidBtn();
    initInput();
    end();
}

void PatternPanel::initControlRowLayout()
{
    // Group ctors called begin()/end() internally; controlRow is still current.
    controlRow.gap(sg);
    controlRow.margin(pad, 2, pad, 2);

    controlRow.fixed(&recentreBtn,     recentreBtnW);
    controlRow.fixed(&patternName,     nameW);
    controlRow.fixed(&outChoice,       outChoiceW);
    controlRow.fixed(&harmonyControls, HarmonyControls::kWidth);
    controlRow.fixed(&timeControls,    TimeControls::kWidth);
    // spacer is flexible — fills space between controls and rapidBtn
    controlRow.fixed(&rapidBtn,        rapidBtnW);
    controlRow.end();

    // Move input after controlRow so it renders on top (z-order)
    remove(&input);
    add(&input);

    box(FL_NO_BOX);
}

void PatternPanel::initPatternName()
{
    recentreBtn.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (self->onFocus) self->onFocus();
    }, this);

    patternName.box(FL_NO_BOX);
    patternName.labelcolor(panelText);
    patternName.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
}

void PatternPanel::initHarmonyControls()
{
    auto& ks = harmonyControls.keySec;
    auto& cs = harmonyControls.chordSec;

    ks.baseLabel.box(FL_NO_BOX);
    ks.baseLabel.labelcolor(panelText);
    ks.baseLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    ks.sharpFlatBtn.color(HarmonyControls::kBg);
    ks.sharpFlatBtn.labelcolor(panelText);
    ks.sharpFlatBtn.setBorderWidth(1);
    ks.sharpFlatBtn.setBorderColor(panelCtrlBorder);
    ks.sharpFlatBtn.onToggle = [this](bool sharp) {
        int idx  = harmonyControls.keySec.rootChoice.value();
        useSharp = sharp;
        updateRootChoiceLabels(idx);
        commitHarmony();
    };

    updateRootChoiceLabels(0);

    auto paramsCb = [](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        self->commitHarmony();
    };
    ks.rootChoice.color(HarmonyControls::kBg);
    ks.rootChoice.setBorderColor(panelCtrlBorder);
    ks.rootChoice.callback(paramsCb, this);

    for (const char* v : {"-1", "0", "1"})
        ks.octaveChoice.add(v);
    ks.octaveChoice.value(1);   // default: 0
    ks.octaveChoice.color(HarmonyControls::kBg);
    ks.octaveChoice.setBorderColor(panelCtrlBorder);
    ks.octaveChoice.tooltip("Octave offset");
    ks.octaveChoice.callback(paramsCb, this);

    cs.chordScaleBtn.color(HarmonyControls::kBg);
    cs.chordScaleBtn.labelcolor(panelText);
    cs.chordScaleBtn.setBorderWidth(1);
    cs.chordScaleBtn.setBorderColor(panelCtrlBorder);
    cs.chordScaleBtn.onToggle = [this](bool chordMode) {
        showScale = !chordMode;
        populateChordChoice();
        harmonyControls.chordSec.chordChoice.value(0);
        commitHarmony();
    };

    populateChordChoice();
    cs.chordChoice.value(0);
    cs.chordChoice.color(HarmonyControls::kBg);
    cs.chordChoice.setBorderColor(panelCtrlBorder);
    cs.chordChoice.callback(paramsCb, this);
}

// The signature just changed: snap the beat definition to the one it implies.
// The beat dropdown's own callback does not do this, so a beat the user picks
// afterwards stays until the signature changes again.
static void snapBeatTo(TimeSigSection& ts, int top, int den)
{
    ts.beatChoice.setBeatUnit(timeSettings::impliedBeatUnit(top, den));
    ts.beatChoice.redraw();
}

void PatternPanel::initTimeControls()
{
    auto& ts = timeControls.timeSigSec;
    auto& bs = timeControls.barsSec;
    auto& ds = timeControls.divSec;

    ts.timeSigLabel.box(FL_NO_BOX);
    ts.timeSigLabel.labelcolor(panelText);
    ts.timeSigLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    ts.timeSigNum.color(TimeControls::kBg);
    ts.timeSigNum.setBorderColor(panelCtrlBorder);
    ts.timeSigNum.textcolor(panelText);
    ts.timeSigNum.cursor_color(panelText);
    ts.timeSigNum.labelcolor(panelText);
    ts.timeSigNum.range(timeSettings::numeratorMin, timeSettings::numeratorMax);
    ts.timeSigNum.step(1);
    ts.timeSigNum.value(timeSettings::numeratorDefault);
    ts.timeSigNum.when(FL_WHEN_RELEASE);
    ts.timeSigNum.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ts   = self->timeControls.timeSigSec;
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.patternIdForSelectedLane();
        int den = timeSettings::denominatorAt(ts.timeSigDen.value());
        int top = (int)ts.timeSigNum.value();
        snapBeatTo(ts, top, den);
        self->pattern->setPatternTimeSig(patId, top, den, ts.beatChoice.beatUnit());
    }, this);

    ts.timeSigSlash.box(FL_NO_BOX);
    ts.timeSigSlash.labelcolor(panelText);

    ts.timeSigDen.color(TimeControls::kBg);
    ts.timeSigDen.labelcolor(panelText);
    ts.timeSigDen.setBorderColor(panelCtrlBorder);
    for (const char* v : timeSettings::denominatorLabels)
        ts.timeSigDen.add(v);
    ts.timeSigDen.value(timeSettings::denominatorDefaultIndex);
    ts.timeSigDen.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ts   = self->timeControls.timeSigSec;
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.patternIdForSelectedLane();
        int den = timeSettings::denominatorAt(ts.timeSigDen.value());
        int top = (int)ts.timeSigNum.value();
        for (const auto& p : tl.patterns) {
            if (p.id == patId && p.timeSigBottom > 0) {
                // Keep the bar the same musical length across a denominator change.
                top = std::clamp((int)std::round((float)p.timeSigTop * den / p.timeSigBottom),
                                 timeSettings::numeratorMin, timeSettings::numeratorMax);
                ts.timeSigNum.value(top);
                break;
            }
        }
        snapBeatTo(ts, top, den);
        self->pattern->setPatternTimeSig(patId, top, den, ts.beatChoice.beatUnit());
    }, this);

    ts.beatChoice.color(TimeControls::kBg);
    ts.beatChoice.labelcolor(panelText);
    ts.beatChoice.textcolor(panelText);
    ts.beatChoice.setBorderColor(panelCtrlBorder);
    ts.beatChoice.tooltip("Beat definition");
    ts.beatChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ts   = self->timeControls.timeSigSec;
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.patternIdForSelectedLane();
        int den   = timeSettings::denominatorAt(ts.timeSigDen.value());
        self->pattern->setPatternTimeSig(patId, (int)ts.timeSigNum.value(), den,
                                         ts.beatChoice.beatUnit());
    }, this);

    bs.barsLabel.box(FL_NO_BOX);
    bs.barsLabel.labelcolor(panelText);
    bs.barsLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    bs.barsInput.color(TimeControls::kBg);
    bs.barsInput.setBorderColor(panelCtrlBorder);
    bs.barsInput.textcolor(panelText);
    bs.barsInput.cursor_color(panelText);
    bs.barsInput.labelcolor(panelText);
    bs.barsInput.range(1, 64);
    bs.barsInput.step(1);
    bs.barsInput.value(2);
    bs.barsInput.when(FL_WHEN_RELEASE);
    bs.barsInput.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& bs   = self->timeControls.barsSec;
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.patternIdForSelectedLane();
        int bars = std::max(1, (int)bs.barsInput.value());
        for (const auto& p : tl.patterns) {
            if (p.id != patId) continue;
            self->pattern->setPatternLength(patId, (float)(bars * p.timeSigTop));
            break;
        }
    }, this);

    auto& zs = timeControls.zoomSec;
    zs.zoomLabel.box(FL_NO_BOX);
    zs.zoomLabel.labelcolor(panelText);
    zs.zoomLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const char* v : {"x1", "x2", "x4"})
        zs.zoomChoice.add(v);
    zs.zoomChoice.value(kZoomDefault);
    zs.zoomChoice.color(TimeControls::kBg);
    zs.zoomChoice.labelcolor(panelText);
    zs.zoomChoice.setBorderColor(panelCtrlBorder);
    zs.zoomChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        int patId = self->selectedPatternId();
        if (patId != 0 && self->pattern)
            self->pattern->setPatternZoom(patId, self->timeControls.zoomSec.zoomChoice.value());
        if (self->onZoomChanged) self->onZoomChanged(self->computeZoomFactor());
    }, this);

    ds.divLabel.box(FL_NO_BOX);
    ds.divLabel.labelcolor(panelText);
    ds.divLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const char* v : {"None", "1\\/2", "1\\/3", "1\\/5", "1\\/7"})
        ds.divChoice.add(v);
    ds.divChoice.value(kDivisionsDefault);
    ds.divChoice.color(TimeControls::kBg);
    ds.divChoice.labelcolor(panelText);
    ds.divChoice.setBorderColor(panelCtrlBorder);
    ds.divChoice.tooltip("Subdivisions of the beat");
    ds.divChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        int patId = self->selectedPatternId();
        if (patId != 0 && self->pattern)
            self->pattern->setPatternDivisions(patId, self->timeControls.divSec.divChoice.value());
        if (self->onSnapChanged)
            self->onSnapChanged(self->computeSnapBeats());
        if (self->onDivisionsChanged)
            self->onDivisionsChanged(self->computeDivisions());
    }, this);

    ds.snapBtn.type(FL_TOGGLE_BUTTON);
    ds.snapBtn.value(1);
    ds.snapBtn.color(kSnapActiveColor);
    ds.snapBtn.labelcolor(panelText);
    ds.snapBtn.setBorderWidth(1);
    ds.snapBtn.setBorderColor(panelCtrlBorder);
    ds.snapBtn.tooltip("Snap new notes and resized edges to the divisions");
    ds.snapBtn.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ds   = self->timeControls.divSec;
        bool on    = ds.snapBtn.value() != 0;
        ds.snapBtn.color(on ? kSnapActiveColor : TimeControls::kBg);
        ds.snapBtn.redraw();
        int patId = self->selectedPatternId();
        if (patId != 0 && self->pattern)
            self->pattern->setPatternSnapEnabled(patId, on);
        if (self->onSnapChanged)
            self->onSnapChanged(self->computeSnapBeats());
    }, this);
}

void PatternPanel::initOutChoice()
{
    outChoice.value(0);
    outChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (!self->pattern || !self->instr_) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.patternIdForSelectedLane();
        PatternType type = PatternType::STANDARD;
        for (const auto& p : tl.patterns)
            if (p.id == patId) { type = p.type; break; }
        // Build the filtered instrument list (same filtering as refreshOutChoice)
        std::vector<int> ids;
        for (const auto& i : tl.instruments)
            if (i.isDrum == (type == PatternType::DRUM)) ids.push_back(i.id);
        int idx = self->outChoice.value();
        int instrId = (idx >= 0 && idx < (int)ids.size()) ? ids[idx] : 0;
        self->instr_->setPatternInstrument(patId, instrId);
    }, this);
}

void PatternPanel::initRapidBtn()
{
    rapidBtn.type(FL_TOGGLE_BUTTON);
    rapidBtn.color(panelBg);
    rapidBtn.labelcolor(panelText);
    rapidBtn.setBorderWidth(1);
    rapidBtn.setBorderColor(panelCtrlBorder);
    rapidBtn.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        bool on = self->rapidBtn.value() != 0;
        self->rapidBtn.color(on ? kRapidActiveColor : panelBg);
        self->rapidBtn.redraw();
        if (self->onRapidChanged) self->onRapidChanged(on);
    }, this);
}

void PatternPanel::initInput()
{
    input.hide();
    input.box(FL_FLAT_BOX);
    input.color(0x37415100);
    input.textcolor(panelText);
    input.cursor_color(panelText);
    input.when(FL_WHEN_ENTER_KEY);
    input.callback([](Fl_Widget*, void* d) {
        static_cast<PatternPanel*>(d)->commitEdit();
    }, this);
    input.onUnfocus([this]() { commitEdit(); });
}

void PatternPanel::populateChordChoice()
{
    auto& cc = harmonyControls.chordSec.chordChoice;
    cc.clear();
    for (int i = 0; i < numChordDefs; ++i) {
        if (chordDefs[i].isScale != showScale) continue;
        // A non-empty `submenu` nests the entry under that named submenu ("Sub/name");
        // otherwise it sits at top level. The chordDefs index rides along as the item's
        // user_data so the selection maps back regardless of the submenu structure.
        const char* sub = chordDefs[i].submenu;
        std::string path = (sub && *sub) ? std::string(sub) + "/" + chordDefs[i].name
                                         : chordDefs[i].name;
        cc.add(path.c_str(), 0, nullptr, (void*)(intptr_t)i);
    }
    cc.redraw();
}

void PatternPanel::setParams(int root, std::string_view chordHash, bool sharp, int octaveOffset)
{
    useSharp = sharp;
    harmonyControls.keySec.sharpFlatBtn.set(sharp);
    updateRootChoiceLabels(root);
    harmonyControls.keySec.octaveChoice.value(std::clamp(octaveOffset, -1, 1) + 1);

    int chord = chordIndexForHash(chordHash);
    showScale = chordDefs[chord].isScale;
    harmonyControls.chordSec.chordScaleBtn.set(!showScale);
    populateChordChoice();
    // Select the menu item whose user_data carries this chordDefs index.
    auto& cc = harmonyControls.chordSec.chordChoice;
    cc.value(0);
    for (int i = 0; i < cc.size(); ++i) {
        const Fl_Menu_Item& it = cc.menu()[i];
        if (it.label() && !(it.flags & FL_SUBMENU) &&
            (int)(intptr_t)it.user_data() == chord) {
            cc.value(i);
            break;
        }
    }
    redraw();
}

void PatternPanel::setInstruments(ObservableInstrument* instr)
{
    swapObserver(instr_, instr, this);
    refreshOutChoice();
}

void PatternPanel::refreshOutChoice()
{
    if (!pattern || !instr_) { outChoice.value(0); return; }
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { outChoice.value(0); return; }
    int patId = tl.patternIdForSelectedLane();

    PatternType type        = PatternType::STANDARD;
    int         currentId   = 0;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        type      = p.type;
        currentId = p.instrumentId;
        break;
    }

    // Build the filtered instrument list (only same type as pattern)
    bool isDrum = (type == PatternType::DRUM);
    std::vector<int>         ids;
    std::vector<std::string> names;
    for (const auto& i : tl.instruments) {
        if (i.isDrum != isDrum) continue;
        ids.push_back(i.id);
        names.push_back(i.name);
    }

    outChoice.clear();
    for (const auto& n : names) outChoice.add(n.c_str());

    for (int i = 0; i < (int)ids.size(); i++) {
        if (ids[i] == currentId) {
            outChoice.value(i);
            outChoice.redraw();
            return;
        }
    }

    // Current instrument not in the type-filtered list — assign first available.
    outChoice.value(0);
    if (!ids.empty())
        instr_->setPatternInstrument(patId, ids[0]);
    outChoice.redraw();
}

void PatternPanel::refreshBars()
{
    auto& bi = timeControls.barsSec.barsInput;
    if (!pattern) { bi.value(2); return; }
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { bi.value(2); return; }
    int patId = tl.patternIdForSelectedLane();
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        bi.value(std::max(1, (int)std::round(p.lengthBeats / (float)p.timeSigTop)));
        bi.redraw();
        return;
    }
}

void PatternPanel::refreshTimeSig()
{
    auto& ts = timeControls.timeSigSec;
    auto showDefault = [&ts]() {
        ts.timeSigNum.value(timeSettings::numeratorDefault);
        ts.timeSigDen.value(timeSettings::denominatorDefaultIndex);
        ts.beatChoice.value(timeSettings::beatUnitDefaultIndex);
    };
    if (!pattern) { showDefault(); return; }
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { showDefault(); return; }
    int patId = tl.patternIdForSelectedLane();
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        ts.timeSigNum.value(p.timeSigTop);
        ts.timeSigDen.value(timeSettings::denominatorIndex(p.timeSigBottom));
        ts.beatChoice.setBeatUnit(p.beat);
        ts.timeSigNum.redraw();
        ts.timeSigDen.redraw();
        ts.beatChoice.redraw();
        return;
    }
}

void PatternPanel::commitHarmony()
{
    int patId = selectedPatternId();
    if (patId != 0 && pattern) {
        // Capture the pattern's current chord before overwriting it: an interactive
        // chord change to a different size must remap this pattern's notes to fit.
        std::string oldHash;
        for (const auto& p : pattern->get().patterns)
            if (p.id == patId) { oldHash = p.chordHash; break; }
        std::string newHash = chordHash();
        pattern->setPatternHarmony(patId, rootPitch(), newHash, useSharp, octaveOffset());
        int oldSize = chordDefForHash(oldHash).size;
        int newSize = chordDefForHash(newHash).size;
        if (oldSize != newSize)
            pattern->remapPatternNotes(patId, oldSize, newSize);
    }
    if (onParamsChanged) onParamsChanged();
}

void PatternPanel::refreshHarmony()
{
    if (!pattern) return;
    int patId = selectedPatternId();
    for (const auto& p : pattern->get().patterns) {
        if (p.id != patId) continue;
        setParams(p.rootPitch, p.chordHash, p.useSharp, p.octaveOffset);
        return;
    }
}

void PatternPanel::refreshDivisions()
{
    if (!pattern) return;
    int patId = selectedPatternId();
    auto& ds  = timeControls.divSec;
    for (const auto& p : pattern->get().patterns) {
        if (p.id != patId) continue;
        ds.divChoice.value(p.divisions);
        ds.snapBtn.value(p.snapEnabled ? 1 : 0);
        ds.snapBtn.color(p.snapEnabled ? kSnapActiveColor : TimeControls::kBg);
        ds.divChoice.redraw();
        ds.snapBtn.redraw();
        return;
    }
}

void PatternPanel::refreshZoom()
{
    if (!pattern) return;
    int patId = selectedPatternId();
    for (const auto& p : pattern->get().patterns) {
        if (p.id != patId) continue;
        timeControls.zoomSec.zoomChoice.value(p.zoom);
        timeControls.zoomSec.zoomChoice.redraw();
        return;
    }
}

void PatternPanel::updateRootChoiceLabels(int idx)
{
    auto& rc = harmonyControls.keySec.rootChoice;
    rc.clear();
    if (useSharp)
        for (const char* n : {"A","A#","B","C","C#","D","D#","E","F","F#","G","G#"})
            rc.add(n);
    else
        for (const char* n : {"A","Bb","B","C","Db","D","Eb","E","F","Gb","G","Ab"})
            rc.add(n);
    rc.value(idx);
    rc.redraw();
}

PatternPanel::~PatternPanel()
{
    swapObserver(pattern, nullptr, this);
}

void PatternPanel::setPattern(ObservablePattern* tl)
{
    swapObserver(pattern, tl, this);
    onTimelineChanged();
}

void PatternPanel::configureStandardRow()
{
    harmonyControls.show();
    rapidBtn.show();
    timeControls.zoomSec.zoomChoice.activate();
    controlRow.resize(controlRow.x(), controlRow.y(), controlRow.w(), controlRow.h());
    redraw();
}

void PatternPanel::configureDrumRow()
{
    harmonyControls.hide();
    if (rapidBtn.value()) {
        rapidBtn.value(0);
        rapidBtn.color(panelBg);
        if (onRapidChanged) onRapidChanged(false);
    }
    rapidBtn.hide();
    timeControls.zoomSec.zoomChoice.activate();
    controlRow.resize(controlRow.x(), controlRow.y(), controlRow.w(), controlRow.h());
    redraw();
}

void PatternPanel::configurePianorollRow()
{
    harmonyControls.hide();
    if (rapidBtn.value()) {
        rapidBtn.value(0);
        rapidBtn.color(panelBg);
        if (onRapidChanged) onRapidChanged(false);
    }
    rapidBtn.hide();
    timeControls.zoomSec.zoomChoice.activate();
    controlRow.resize(controlRow.x(), controlRow.y(), controlRow.w(), controlRow.h());
    redraw();
}

void PatternPanel::onTimelineChanged()
{
    if (!pattern) return;
    const auto& tl  = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel >= 0 && sel < (int)tl.tracks.size()) {
        const auto& selTrack = tl.tracks[sel];
        int patId = tl.patternIdForSelectedLane();
        PatternType type = PatternType::STANDARD;
        std::string pName;
        for (const auto& p : tl.patterns)
            if (p.id == patId) { type = p.type; pName = p.name; break; }
        patternName.copy_label(pName.c_str());
        switch (type) {
        case PatternType::DRUM:      configureDrumRow();      break;
        case PatternType::PIANOROLL: configurePianorollRow(); break;
        default:                     configureStandardRow();  break;
        }
    } else {
        patternName.copy_label("");
        configureStandardRow();
    }
    refreshOutChoice();
    refreshTimeSig();
    refreshBars();
    refreshHarmony();
    refreshDivisions();
    refreshZoom();
    // Push the freshly-loaded pattern's harmony/divisions/zoom to the editors so
    // switching patterns reinterprets pitches, snapping and zoom for the newly
    // selected pattern.
    if (onParamsChanged)      onParamsChanged();
    if (onSnapChanged)        onSnapChanged(computeSnapBeats());
    if (onDivisionsChanged)   onDivisionsChanged(computeDivisions());
    if (onZoomChanged)        onZoomChanged(computeZoomFactor());
    redraw();
}

void PatternPanel::startEdit()
{
    if (!pattern) return;
    const auto& tl  = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) return;

    // Edit the pattern currently shown (the selected lane), not always lane 0 —
    // this must match the id used to display the name in onTimelineChanged().
    int patId = tl.patternIdForSelectedLane();
    if (patId <= 0) return;
    editingPatId = patId;
    originalLabel.clear();
    for (const auto& p : tl.patterns)
        if (p.id == patId) { originalLabel = p.name; break; }
    input.resize(patternName.x(), patternName.y(), patternName.w(), patternName.h());
    input.value(originalLabel.c_str());
    input.textcolor(panelText);
    input.show();
    input.take_focus();
    input.position(input.size(), 0);  // select all
    input.onChange([this]() { checkDuplicate(); });
    InlineEditDispatch::install(this, [this]() { commitEdit(); });
    redraw();
}

void PatternPanel::checkDuplicate()
{
    bool dup = pattern && editingPatId >= 0 &&
               pattern->song()->patternNameCollides(editingPatId, input.value());
    input.textcolor(dup ? FL_RED : panelText);
    input.redraw();
}

void PatternPanel::commitEdit()
{
    if (editingPatId < 0) return;
    int id       = editingPatId;
    editingPatId = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = input.value();
    input.hide();
    if (pattern)
        pattern->song()->setPatternName(id, newLabel);
    redraw();
}

void PatternPanel::cancelEdit()
{
    if (editingPatId < 0) return;
    editingPatId = -1;
    InlineEditDispatch::uninstall();
    input.hide();
    redraw();
}

void PatternPanel::resize(int x, int y, int w, int h)
{
    Fl_Widget::resize(x, y, w, h);
    controlRow.resize(x, y, w, h);
    if (editingPatId >= 0)
        input.resize(patternName.x(), patternName.y(), patternName.w(), patternName.h());
}

void PatternPanel::draw()
{
    fl_color(panelBorder);
    fl_rectf(x(), y(), w(), 1);
    fl_color(panelBg);
    fl_rectf(x(), y() + 1, w(), h() - 1);
    draw_children();
}

int PatternPanel::handle(int event)
{
    if (Fl_Group::handle(event)) return 1;

    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
        int ex = Fl::event_x(), ey = Fl::event_y();
        bool onName = ex >= patternName.x() && ex < patternName.x() + patternName.w()
                   && ey >= patternName.y() && ey < patternName.y() + patternName.h();
        if (onName && Fl::event_clicks() == 1) {
            startEdit();
            return 1;
        }
    }

    if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape && editingPatId >= 0) {
        cancelEdit();
        return 1;
    }

    return 0;
}
