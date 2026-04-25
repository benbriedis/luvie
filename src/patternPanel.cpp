#include "patternPanel.hpp"
#include "chords.hpp"
#include "panelStyle.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <cmath>

static constexpr int pad          = 3;
static constexpr int sg           = 3;    // small gap within base group
static constexpr int groupGap     = 12;   // between base group and chord
static constexpr int ctrlH        = 24;
static constexpr int labelW       = 55;
static constexpr int nameW        = 150;
static constexpr int toggleBtnW   = 26;
static constexpr int rootChoiceW  = 110;
static constexpr int choiceW      = 130;
static constexpr int timeSigLabelW = 28;
static constexpr int timeSigNumW   = 40;
static constexpr int slashW        = 12;
static constexpr int timeSigDenW   = 50;
static constexpr int barsLabelW    = 36;
static constexpr int barsInputW    = 40;
static constexpr int outLabelW    = 28;
static constexpr int outChoiceW   = 155;

static int nameX(int x)          { return x + pad; }
static int baseLabelX(int x)     { return nameX(x) + nameW + pad; }
static int sharpFlatBtnX(int x)  { return baseLabelX(x) + labelW + sg; }
static int rootChoiceX(int x)    { return sharpFlatBtnX(x) + toggleBtnW + sg; }
static int chordLabelX(int x)    { return rootChoiceX(x) + rootChoiceW + groupGap; }
static int chordChoiceX(int x)   { return chordLabelX(x) + labelW; }
static int timeSigLabelX(int x)  { return chordChoiceX(x) + choiceW + groupGap; }
static int timeSigNumX(int x)    { return timeSigLabelX(x) + timeSigLabelW; }
static int timeSigSlashX(int x)  { return timeSigNumX(x) + timeSigNumW; }
static int timeSigDenX(int x)    { return timeSigSlashX(x) + slashW; }
static int barsLabelX(int x)     { return timeSigDenX(x) + timeSigDenW + groupGap; }
static int barsInputX(int x)     { return barsLabelX(x) + barsLabelW; }
static int outLabelX(int x)      { return barsInputX(x) + barsInputW + groupGap; }
static int outChoiceX(int x)     { return outLabelX(x) + outLabelW; }
static int ctrlY(int y, int h)   { return y + (h - ctrlH) / 2; }

PatternPanel::PatternPanel(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      patternName (nameX(x),          ctrlY(y,h), nameW,         ctrlH),
      baseLabel   (baseLabelX(x),     ctrlY(y,h), labelW,        ctrlH, "Base"),
      sharpFlatBtn(sharpFlatBtnX(x),  ctrlY(y,h), toggleBtnW,    ctrlH, "#"),
      rootChoice  (rootChoiceX(x),    ctrlY(y,h), rootChoiceW,   ctrlH),
      chordLabel  (chordLabelX(x),    ctrlY(y,h), labelW,        ctrlH, "Chord"),
      chordChoice (chordChoiceX(x),   ctrlY(y,h), choiceW,       ctrlH),
      timeSigLabel(timeSigLabelX(x),  ctrlY(y,h), timeSigLabelW, ctrlH, "Sig"),
      timeSigNum  (timeSigNumX(x),    ctrlY(y,h), timeSigNumW,   ctrlH),
      timeSigSlash(timeSigSlashX(x),  ctrlY(y,h), slashW,        ctrlH, "/"),
      timeSigDen  (timeSigDenX(x),    ctrlY(y,h), timeSigDenW,   ctrlH),
      barsLabel   (barsLabelX(x),    ctrlY(y,h), barsLabelW,    ctrlH, "Bars"),
      barsInput   (barsInputX(x),    ctrlY(y,h), barsInputW,    ctrlH),
      outLabel    (outLabelX(x),      ctrlY(y,h), outLabelW,     ctrlH, "Out"),
      outChoice   (outChoiceX(x),     ctrlY(y,h), outChoiceW,    ctrlH),
      input       (nameX(x),          ctrlY(y,h), nameW,         ctrlH)
{
    box(FL_NO_BOX);

    patternName.box(FL_NO_BOX);
    patternName.labelcolor(panelText);
    patternName.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    baseLabel.box(FL_NO_BOX);
    baseLabel.labelcolor(panelText);
    baseLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    sharpFlatBtn.color(panelBg);
    sharpFlatBtn.labelcolor(panelText);
    sharpFlatBtn.setBorderWidth(1);
    sharpFlatBtn.setBorderColor(panelCtrlBorder);

    sharpFlatBtn.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        int idx = self->rootChoice.value();
        self->useSharp = !self->useSharp;
        self->sharpFlatBtn.label(self->useSharp ? "#" : "b");
        self->updateRootChoiceLabels(idx);
        if (self->onParamsChanged) self->onParamsChanged();
    }, this);

    updateRootChoiceLabels(0);

    auto paramsCb = [](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (self->onParamsChanged) self->onParamsChanged();
    };
    rootChoice.callback(paramsCb, this);

    chordLabel.box(FL_NO_BOX);
    chordLabel.labelcolor(panelText);
    chordLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const auto& def : chordDefs)
        chordChoice.add(def.name);
    chordChoice.value(0);
    chordChoice.callback(paramsCb, this);

    timeSigLabel.box(FL_NO_BOX);
    timeSigLabel.labelcolor(panelText);
    timeSigLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    timeSigNum.box(FL_FLAT_BOX);
    timeSigNum.color(panelBorder);
    timeSigNum.textcolor(panelText);
    timeSigNum.cursor_color(panelText);
    timeSigNum.labelcolor(panelText);
    timeSigNum.range(1, 32);
    timeSigNum.step(1);
    timeSigNum.value(4);
    timeSigNum.when(FL_WHEN_RELEASE);
    timeSigNum.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (!self->timeline) return;
        const auto& tl = self->timeline->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
        int den = (self->timeSigDen.value() >= 0) ? denoms[self->timeSigDen.value()] : 4;
        self->timeline->setPatternTimeSig(patId, (int)self->timeSigNum.value(), den);
    }, this);

    timeSigSlash.box(FL_NO_BOX);
    timeSigSlash.labelcolor(panelText);

    timeSigDen.color(panelBorder);
    timeSigDen.labelcolor(panelText);
    timeSigDen.setBorderColor(panelCtrlBorder);
    for (const char* v : {"1", "2", "4", "8", "16", "32"})
        timeSigDen.add(v);
    timeSigDen.value(2); // default: 4
    timeSigDen.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (!self->timeline) return;
        const auto& tl = self->timeline->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
        int den = (self->timeSigDen.value() >= 0) ? denoms[self->timeSigDen.value()] : 4;
        self->timeline->setPatternTimeSig(patId, (int)self->timeSigNum.value(), den);
    }, this);

    barsLabel.box(FL_NO_BOX);
    barsLabel.labelcolor(panelText);
    barsLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    barsInput.box(FL_FLAT_BOX);
    barsInput.color(panelBorder);
    barsInput.textcolor(panelText);
    barsInput.cursor_color(panelText);
    barsInput.labelcolor(panelText);
    barsInput.range(1, 64);
    barsInput.step(1);
    barsInput.value(2);
    barsInput.when(FL_WHEN_RELEASE);
    barsInput.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (!self->timeline) return;
        const auto& tl = self->timeline->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        int bars = std::max(1, (int)self->barsInput.value());
        for (const auto& p : tl.patterns) {
            if (p.id != patId) continue;
            self->timeline->setPatternLength(patId, (float)(bars * p.timeSigTop));
            break;
        }
    }, this);

    outLabel.box(FL_NO_BOX);
    outLabel.labelcolor(panelText);
    outLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    outChoice.value(0);
    outChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (!self->timeline) return;
        const auto& tl = self->timeline->get();
        int sel = tl.selectedTrackIndex;
        if (sel < 0 || sel >= (int)tl.tracks.size()) return;
        int patId = tl.tracks[sel].patternId;
        // Determine which list is currently shown
        PatternType type = PatternType::STANDARD;
        for (const auto& p : tl.patterns)
            if (p.id == patId) { type = p.type; break; }
        const auto& names = (type == PatternType::DRUM)
            ? self->drumChannelNames_ : self->stdChannelNames_;
        int idx = self->outChoice.value();
        std::string name = (idx >= 0 && idx < (int)names.size()) ? names[idx] : "";
        self->timeline->setPatternOutputChannel(patId, name);
    }, this);

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

    end();
}

void PatternPanel::setParams(int root, int chord, bool sharp)
{
	useSharp = sharp;
	sharpFlatBtn.label(useSharp ? "#" : "b");
	updateRootChoiceLabels(root);
	chordChoice.value(chord);
	redraw();
}

void PatternPanel::setChannels(const std::vector<std::string>& stdNames,
                               const std::vector<std::string>& drumNames)
{
    stdChannelNames_  = stdNames;
    drumChannelNames_ = drumNames;
    refreshOutChoice();
}

void PatternPanel::refreshOutChoice()
{
    if (!timeline) { outChoice.value(0); return; }
    const auto& tl = timeline->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { outChoice.value(0); return; }
    int patId = tl.tracks[sel].patternId;

    PatternType type = PatternType::STANDARD;
    std::string currentChannel;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        type           = p.type;
        currentChannel = p.outputChannelName;
        break;
    }

    const auto& names = (type == PatternType::DRUM) ? drumChannelNames_ : stdChannelNames_;

    outChoice.clear();
    for (const auto& n : names) outChoice.add(n.c_str());

    for (int i = 0; i < (int)names.size(); i++) {
        if (names[i] == currentChannel) {
            outChoice.value(i);
            outChoice.redraw();
            return;
        }
    }

    // Current channel not in the type-filtered list — assign first available.
    outChoice.value(0);
    if (!names.empty())
        timeline->setPatternOutputChannel(patId, names[0]);
    outChoice.redraw();
}

void PatternPanel::refreshBars()
{
    if (!timeline) { barsInput.value(2); return; }
    const auto& tl = timeline->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { barsInput.value(2); return; }
    int patId = tl.tracks[sel].patternId;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        barsInput.value(std::max(1, (int)std::round(p.lengthBeats / (float)p.timeSigTop)));
        barsInput.redraw();
        return;
    }
}

void PatternPanel::refreshTimeSig()
{
    static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
    if (!timeline) { timeSigNum.value(4); timeSigDen.value(2); return; }
    const auto& tl = timeline->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) { timeSigNum.value(4); timeSigDen.value(2); return; }
    int patId = tl.tracks[sel].patternId;
    for (const auto& p : tl.patterns) {
        if (p.id != patId) continue;
        timeSigNum.value(p.timeSigTop);
        int idx = 2;
        for (int i = 0; i < 6; ++i) if (denoms[i] == p.timeSigBottom) { idx = i; break; }
        timeSigDen.value(idx);
        timeSigNum.redraw();
        timeSigDen.redraw();
        return;
    }
}

void PatternPanel::updateRootChoiceLabels(int idx)
{
    rootChoice.clear();
    if (useSharp)
        for (const char* n : {"A","A#","B","C","C#","D","D#","E","F","F#","G","G#"})
            rootChoice.add(n);
    else
        for (const char* n : {"A","Bb","B","C","Db","D","Eb","E","F","Gb","G","Ab"})
            rootChoice.add(n);
    rootChoice.value(idx);
    rootChoice.redraw();
}

PatternPanel::~PatternPanel()
{
    swapObserver(timeline, nullptr, this);
}

void PatternPanel::setTimeline(ObservableTimeline* tl)
{
    swapObserver(timeline, tl, this);
    onTimelineChanged();
}

void PatternPanel::setDrumMode(bool drum)
{
    if (drum) {
        baseLabel.hide();
        sharpFlatBtn.hide();
        rootChoice.hide();
        chordLabel.hide();
        chordChoice.hide();
    } else {
        baseLabel.show();
        sharpFlatBtn.show();
        rootChoice.show();
        chordLabel.show();
        chordChoice.show();
    }
    redraw();
}

void PatternPanel::onTimelineChanged()
{
    if (!timeline) return;
    const auto& tl  = timeline->get();
    int sel = tl.selectedTrackIndex;
    if (sel >= 0 && sel < (int)tl.tracks.size()) {
        patternName.copy_label(tl.tracks[sel].label.c_str());
        int patId = tl.tracks[sel].patternId;
        bool hideChord = false;
        for (const auto& p : tl.patterns)
            if (p.id == patId) { hideChord = (p.type == PatternType::DRUM || p.type == PatternType::PIANOROLL); break; }
        setDrumMode(hideChord);
    } else {
        patternName.copy_label("");
        setDrumMode(false);
    }
    refreshOutChoice();
    refreshTimeSig();
    refreshBars();
    redraw();
}

void PatternPanel::startEdit()
{
    if (!timeline) return;
    const auto& tl  = timeline->get();
    int sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) return;

    editingTrackId = tl.tracks[sel].id;
    originalLabel  = tl.tracks[sel].label;
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
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
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
    if (timeline) {
        const auto& tracks = timeline->get().tracks;
        bool dup = false;
        for (const auto& t : tracks)
            if (t.id != id && t.label == newLabel) { dup = true; break; }
        timeline->renameTrack(id, dup ? originalLabel : newLabel);
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
	int dx = x - this->x(), dy = y - this->y();
	Fl_Widget::resize(x, y, w, h);
	if (dx || dy)
		for (int i = 0; i < children(); i++) {
			Fl_Widget* c = child(i);
			c->position(c->x() + dx, c->y() + dy);
		}
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
