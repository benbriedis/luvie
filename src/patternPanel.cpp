#include "patternPanel.hpp"
#include "chords.hpp"
#include "panelStyle.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
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
static constexpr int snapLabelW    = 40;
static constexpr int snapChoiceW   = 70;
static constexpr int outChoiceW    = 155;
static constexpr int rapidBtnW     = 52;
static constexpr Fl_Color kRapidActiveColor = 0x3B82F600;

static constexpr int kSnapNoteDenoms[] = { 4, 8, 16, 32, 0 };  // 0 = Free
static constexpr int kSnapDefault      = 2;  // 1/16

// ---------------------------------------------------------------------------
// Section struct constructors
// ---------------------------------------------------------------------------

KeySection::KeySection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      baseLabel   (0, 0, kLabelW,  h, "Base"),
      sharpFlatBtn(0, 0, kBtnW,    h, "#"),
      rootChoice  (0, 0, kChoiceW, h)
{
    gap(kGap);
    fixed(&baseLabel,    kLabelW);
    fixed(&sharpFlatBtn, kBtnW);
    fixed(&rootChoice,   kChoiceW);
    end();
}

ChordSection::ChordSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      chordLabel (0, 0, kLabelW,  h, "Chord"),
      chordChoice(0, 0, kChoiceW, h)
{
    gap(kGap);
    fixed(&chordLabel,  kLabelW);
    fixed(&chordChoice, kChoiceW);
    end();
}

TimeSigSection::TimeSigSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      timeSigLabel(0, 0, kLabelW, h, "Sig"),
      timeSigNum  (0, 0, kNumW,   h),
      timeSigSlash(0, 0, kSlashW, h, "/"),
      timeSigDen  (0, 0, kDenW,   h)
{
    gap(kGap);
    fixed(&timeSigLabel, kLabelW);
    fixed(&timeSigNum,   kNumW);
    fixed(&timeSigSlash, kSlashW);
    fixed(&timeSigDen,   kDenW);
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

SnapSection::SnapSection(int x, int y, int h)
    : Fl_Flex(x, y, kWidth, h, Fl_Flex::HORIZONTAL),
      snapLabel (0, 0, kLabelW,  h, "Snap"),
      snapChoice(0, 0, kChoiceW, h)
{
    gap(kGap);
    fixed(&snapLabel,  kLabelW);
    fixed(&snapChoice, kChoiceW);
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
      snapSec   (0, 0, h)
{
    gap(kGap);
    margin(kMargin, 0, kMargin, 0);
    fixed(&timeSigSec, TimeSigSection::kWidth);
    fixed(&barsSec,    BarsSection::kWidth);
    fixed(&snapSec,    SnapSection::kWidth);
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

float PatternPanel::computeSnapBeats() const
{
    int idx = timeControls.snapSec.snapChoice.value();
    if (idx < 0 || idx >= 5) return 0.0f;
    int noteDenom = kSnapNoteDenoms[idx];
    if (noteDenom == 0) return 0.0f;
    static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
    int tsIdx   = timeControls.timeSigSec.timeSigDen.value();
    int tsDenom = (tsIdx >= 0 && tsIdx < 6) ? denoms[tsIdx] : 4;
    return (float)tsDenom / (float)noteDenom;
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

    ks.sharpFlatBtn.color(panelBg);
    ks.sharpFlatBtn.labelcolor(panelText);
    ks.sharpFlatBtn.setBorderWidth(1);
    ks.sharpFlatBtn.setBorderColor(panelCtrlBorder);
    ks.sharpFlatBtn.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ks   = self->harmonyControls.keySec;
        int idx    = ks.rootChoice.value();
        self->useSharp = !self->useSharp;
        ks.sharpFlatBtn.label(self->useSharp ? "#" : "b");
        self->updateRootChoiceLabels(idx);
        if (self->onParamsChanged) self->onParamsChanged();
    }, this);

    updateRootChoiceLabels(0);

    auto paramsCb = [](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (self->onParamsChanged) self->onParamsChanged();
    };
    ks.rootChoice.callback(paramsCb, this);

    cs.chordLabel.box(FL_NO_BOX);
    cs.chordLabel.labelcolor(panelText);
    cs.chordLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const auto& def : chordDefs)
        cs.chordChoice.add(def.name);
    cs.chordChoice.value(0);
    cs.chordChoice.callback(paramsCb, this);
}

void PatternPanel::initTimeControls()
{
    auto& ts = timeControls.timeSigSec;
    auto& bs = timeControls.barsSec;
    auto& ss = timeControls.snapSec;

    ts.timeSigLabel.box(FL_NO_BOX);
    ts.timeSigLabel.labelcolor(panelText);
    ts.timeSigLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    ts.timeSigNum.box(FL_FLAT_BOX);
    ts.timeSigNum.color(panelBorder);
    ts.timeSigNum.textcolor(panelText);
    ts.timeSigNum.cursor_color(panelText);
    ts.timeSigNum.labelcolor(panelText);
    ts.timeSigNum.range(1, 32);
    ts.timeSigNum.step(1);
    ts.timeSigNum.value(4);
    ts.timeSigNum.when(FL_WHEN_RELEASE);
    ts.timeSigNum.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ts   = self->timeControls.timeSigSec;
        static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        int den = (ts.timeSigDen.value() >= 0) ? denoms[ts.timeSigDen.value()] : 4;
        self->pattern->setPatternTimeSig(patId, (int)ts.timeSigNum.value(), den);
        if (self->onSnapChanged) self->onSnapChanged(self->computeSnapBeats());
    }, this);

    ts.timeSigSlash.box(FL_NO_BOX);
    ts.timeSigSlash.labelcolor(panelText);

    ts.timeSigDen.color(panelBorder);
    ts.timeSigDen.labelcolor(panelText);
    ts.timeSigDen.setBorderColor(panelCtrlBorder);
    for (const char* v : {"1", "2", "4", "8", "16", "32"})
        ts.timeSigDen.add(v);
    ts.timeSigDen.value(2); // default: /4
    ts.timeSigDen.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        auto& ts   = self->timeControls.timeSigSec;
        static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        int den = (ts.timeSigDen.value() >= 0) ? denoms[ts.timeSigDen.value()] : 4;
        self->pattern->setPatternTimeSig(patId, (int)ts.timeSigNum.value(), den);
        if (self->onSnapChanged) self->onSnapChanged(self->computeSnapBeats());
    }, this);

    bs.barsLabel.box(FL_NO_BOX);
    bs.barsLabel.labelcolor(panelText);
    bs.barsLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    bs.barsInput.box(FL_FLAT_BOX);
    bs.barsInput.color(panelBorder);
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
        int patId = tl.tracks[sel].patternId;
        int bars = std::max(1, (int)bs.barsInput.value());
        for (const auto& p : tl.patterns) {
            if (p.id != patId) continue;
            self->pattern->setPatternLength(patId, (float)(bars * p.timeSigTop));
            break;
        }
    }, this);

    ss.snapLabel.box(FL_NO_BOX);
    ss.snapLabel.labelcolor(panelText);
    ss.snapLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const char* v : {"1\\/4", "1\\/8", "1\\/16", "1\\/32", "Free"})
        ss.snapChoice.add(v);
    ss.snapChoice.value(kSnapDefault);
    ss.snapChoice.color(panelBorder);
    ss.snapChoice.labelcolor(panelText);
    ss.snapChoice.setBorderColor(panelCtrlBorder);
    ss.snapChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (self->onSnapChanged)
            self->onSnapChanged(self->computeSnapBeats());
    }, this);
}

void PatternPanel::initOutChoice()
{
    outChoice.value(0);
    outChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (!self->pattern) return;
        const auto& tl = self->pattern->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        PatternType type = PatternType::STANDARD;
        for (const auto& p : tl.patterns)
            if (p.id == patId) { type = p.type; break; }
        const auto& names = (type == PatternType::DRUM)
            ? self->drumInstrumentNames_ : self->stdInstrumentNames_;
        int idx = self->outChoice.value();
        std::string name = (idx >= 0 && idx < (int)names.size()) ? names[idx] : "";
        self->pattern->setPatternOutputInstrument(patId, name);
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

void PatternPanel::setParams(int root, int chord, bool sharp)
{
    useSharp = sharp;
    harmonyControls.keySec.sharpFlatBtn.label(useSharp ? "#" : "b");
    updateRootChoiceLabels(root);
    harmonyControls.chordSec.chordChoice.value(chord);
    redraw();
}

void PatternPanel::setInstruments(const std::vector<std::string>& stdNames,
                                  const std::vector<std::string>& drumNames)
{
    stdInstrumentNames_  = stdNames;
    drumInstrumentNames_ = drumNames;
    refreshOutChoice();
}

void PatternPanel::refreshOutChoice()
{
    if (!pattern) { outChoice.value(0); return; }
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { outChoice.value(0); return; }
    int patId = tl.tracks[sel].patternId;

    PatternType type = PatternType::STANDARD;
    std::string currentInstrument;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        type              = p.type;
        currentInstrument = p.outputInstrumentName;
        break;
    }

    const auto& names = (type == PatternType::DRUM) ? drumInstrumentNames_ : stdInstrumentNames_;

    outChoice.clear();
    for (const auto& n : names) outChoice.add(n.c_str());

    for (int i = 0; i < (int)names.size(); i++) {
        if (names[i] == currentInstrument) {
            outChoice.value(i);
            outChoice.redraw();
            return;
        }
    }

    // Current instrument not in the type-filtered list — assign first available.
    outChoice.value(0);
    if (!names.empty())
        pattern->setPatternOutputInstrument(patId, names[0]);
    outChoice.redraw();
}

void PatternPanel::refreshBars()
{
    auto& bi = timeControls.barsSec.barsInput;
    if (!pattern) { bi.value(2); return; }
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { bi.value(2); return; }
    int patId = tl.tracks[sel].patternId;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        bi.value(std::max(1, (int)std::round(p.lengthBeats / (float)p.timeSigTop)));
        bi.redraw();
        return;
    }
}

void PatternPanel::refreshTimeSig()
{
    static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
    auto& ts = timeControls.timeSigSec;
    if (!pattern) { ts.timeSigNum.value(4); ts.timeSigDen.value(2); return; }
    const auto& tl = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) {
        ts.timeSigNum.value(4); ts.timeSigDen.value(2); return;
    }
    int patId = tl.tracks[sel].patternId;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        ts.timeSigNum.value(p.timeSigTop);
        int idx = 2;
        for (int i = 0; i < 6; ++i) if (denoms[i] == p.timeSigBottom) { idx = i; break; }
        ts.timeSigDen.value(idx);
        ts.timeSigNum.redraw();
        ts.timeSigDen.redraw();
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
    controlRow.resize(controlRow.x(), controlRow.y(), controlRow.w(), controlRow.h());
    redraw();
}

void PatternPanel::onTimelineChanged()
{
    if (!pattern) return;
    const auto& tl  = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel >= 0 && sel < (int)tl.tracks.size()) {
        patternName.copy_label(tl.tracks[sel].label.c_str());
        int patId = tl.tracks[sel].patternId;
        PatternType type = PatternType::STANDARD;
        for (const auto& p : tl.patterns)
            if (p.id == patId) { type = p.type; break; }
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
    redraw();
}

void PatternPanel::startEdit()
{
    if (!pattern) return;
    const auto& tl  = pattern->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) return;

    editingTrackId = tl.tracks[sel].id;
    originalLabel  = tl.tracks[sel].label;
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
    if (!pattern) return;
    const auto& tracks = pattern->get().tracks;
    std::string current = input.value();
    bool dup = false;
    for (const auto& t : tracks)
        if (t.id != editingTrackId && t.label == current) { dup = true; break; }
    input.textcolor(dup ? FL_RED : panelText);
    input.redraw();
}

void PatternPanel::commitEdit()
{
    if (editingTrackId < 0) return;
    int id        = editingTrackId;
    editingTrackId = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = input.value();
    input.hide();
    if (pattern) {
        const auto& tracks = pattern->get().tracks;
        bool dup = false;
        for (const auto& t : tracks)
            if (t.id != id && t.label == newLabel) { dup = true; break; }
        pattern->song()->renameTrack(id, dup ? originalLabel : newLabel);
    }
    redraw();
}

void PatternPanel::cancelEdit()
{
    if (editingTrackId < 0) return;
    editingTrackId = -1;
    InlineEditDispatch::uninstall();
    input.hide();
    redraw();
}

void PatternPanel::resize(int x, int y, int w, int h)
{
    Fl_Widget::resize(x, y, w, h);
    controlRow.resize(x, y, w, h);
    if (editingTrackId >= 0)
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

    if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape && editingTrackId >= 0) {
        cancelEdit();
        return 1;
    }

    return 0;
}
