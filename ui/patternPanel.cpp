#include "patternPanel.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>

static constexpr int pad           = 3;
static constexpr int groupGap      = 10;  // between Octave choice and #/b button
static constexpr int ctrlH         = 24;
static constexpr int labelW        = 55;
static constexpr int nameW         = 150;
static constexpr int octaveChoiceW = 50;
static constexpr int toggleBtnW    = 26;
static constexpr int choiceW       = 130;

static int nameX(int x)         { return x + pad; }
static int octaveLabelX(int x)  { return nameX(x) + nameW + pad; }
static int octaveChoiceX(int x) { return octaveLabelX(x) + labelW; }
static int sharpFlatBtnX(int x) { return octaveChoiceX(x) + octaveChoiceW + groupGap; }
static int rootLabelX(int x)    { return sharpFlatBtnX(x) + toggleBtnW + 1; }
static int rootChoiceX(int x)   { return rootLabelX(x) + labelW; }
static int chordLabelX(int x)   { return rootChoiceX(x) + choiceW + pad; }
static int chordChoiceX(int x)  { return chordLabelX(x) + labelW; }
static int ctrlY(int y, int h)  { return y + (h - ctrlH) / 2; }

PatternPanel::PatternPanel(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      patternName (nameX(x),          ctrlY(y,h), nameW,        ctrlH),
      octaveLabel (octaveLabelX(x),  ctrlY(y,h), labelW,       ctrlH, "Octave"),
      octaveChoice(octaveChoiceX(x), ctrlY(y,h), octaveChoiceW,ctrlH),
      sharpFlatBtn(sharpFlatBtnX(x), ctrlY(y,h), toggleBtnW,   ctrlH, "#"),
      rootLabel   (rootLabelX(x),    ctrlY(y,h), labelW,       ctrlH, "Root"),
      rootChoice  (rootChoiceX(x),   ctrlY(y,h), choiceW,      ctrlH),
      chordLabel  (chordLabelX(x),   ctrlY(y,h), labelW,       ctrlH, "Chord"),
      chordChoice (chordChoiceX(x),  ctrlY(y,h), choiceW,      ctrlH),
      input       (nameX(x),           ctrlY(y,h), nameW,        ctrlH)
{
    box(FL_NO_BOX);

    patternName.box(FL_NO_BOX);
    patternName.labelcolor(text);
    patternName.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    octaveLabel.box(FL_NO_BOX);
    octaveLabel.labelcolor(text);
    octaveLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (int i = 0; i <= 9; i++) octaveChoice.add(std::to_string(i).c_str());
    octaveChoice.value(4);

    sharpFlatBtn.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        int idx = self->rootChoice.value();
        self->useSharp = !self->useSharp;
        self->sharpFlatBtn.label(self->useSharp ? "#" : "b");
        self->updateRootChoiceLabels(idx);
        if (self->onParamsChanged) self->onParamsChanged();
    }, this);

    auto paramsCb = [](Fl_Widget*, void* d) {
        auto* self = static_cast<PatternPanel*>(d);
        if (self->onParamsChanged) self->onParamsChanged();
    };
    octaveChoice.callback(paramsCb, this);
    rootChoice.callback(paramsCb, this);
    chordChoice.callback(paramsCb, this);

    rootLabel.box(FL_NO_BOX);
    rootLabel.labelcolor(text);
    rootLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    updateRootChoiceLabels(0);

    chordLabel.box(FL_NO_BOX);
    chordLabel.labelcolor(text);
    chordLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const char* chord : {"Major", "Minor", "Major 7", "Minor 7"})
        chordChoice.add(chord);
    chordChoice.value(0);

    input.hide();
    input.box(FL_FLAT_BOX);
    input.color(0x37415100);
    input.textcolor(text);
    input.cursor_color(text);
    input.when(FL_WHEN_ENTER_KEY);
    input.callback([](Fl_Widget*, void* d) {
        static_cast<PatternPanel*>(d)->commitEdit();
    }, this);
    input.onUnfocus([this]() { commitEdit(); });

    end();
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

void PatternPanel::onTimelineChanged()
{
    if (!timeline) return;
    const auto& tl  = timeline->get();
    int sel = tl.selectedTrackIndex;
    if (sel >= 0 && sel < (int)tl.tracks.size())
        patternName.copy_label(tl.tracks[sel].label.c_str());
    else
        patternName.copy_label("");
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
    input.textcolor(text);
    input.show();
    input.take_focus();
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
    input.textcolor(dup ? FL_RED : text);
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

void PatternPanel::draw()
{
    fl_color(border);
    fl_rectf(x(), y(), w(), 1);
    fl_color(bg);
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
