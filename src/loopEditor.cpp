#include "loopEditor.hpp"
#include "panelStyle.hpp"
#include "appWindow.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// ======================================================
// LoopPanel layout
// ======================================================

static constexpr int lpPad      = 10;
static constexpr int lpCtrlH    = 24;
static constexpr int lpLabelW   = 40;
static constexpr int lpBpmW     = 60;
static constexpr int lpSmallW   = 36;
static constexpr int lpSlashW   = 14;
static constexpr int lpSmGap    = 4;
static constexpr int lpGroupGap = 20;

static int lpCtrlY(int y, int h) { return y + (h - lpCtrlH) / 2; }
static int lpBpmLabelX(int x)   { return x + lpPad; }
static int lpBpmInputX(int x)   { return lpBpmLabelX(x) + lpLabelW + lpSmGap; }
static int lpTsLabelX(int x)    { return lpBpmInputX(x) + lpBpmW + lpGroupGap; }
static int lpTsTopX(int x)      { return lpTsLabelX(x) + lpLabelW + lpSmGap; }
static int lpTsSlashX(int x)    { return lpTsTopX(x) + lpSmallW; }
static int lpTsBotX(int x)      { return lpTsSlashX(x) + lpSlashW; }

// ======================================================
// LoopPanel
// ======================================================

LoopPanel::LoopPanel(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      bpmLabel       (lpBpmLabelX(x),  lpCtrlY(y, h), lpLabelW, lpCtrlH, "BPM"),
      bpmInput       (lpBpmInputX(x),  lpCtrlY(y, h), lpBpmW,   lpCtrlH),
      timeSigLabel   (lpTsLabelX(x),   lpCtrlY(y, h), lpLabelW, lpCtrlH, "Time"),
      timeSigTopInput(lpTsTopX(x),     lpCtrlY(y, h), lpSmallW, lpCtrlH),
      timeSigSlash   (lpTsSlashX(x),   lpCtrlY(y, h), lpSlashW, lpCtrlH, "/"),
      timeSigBotInput(lpTsBotX(x),     lpCtrlY(y, h), lpSmallW, lpCtrlH)
{
    box(FL_NO_BOX);

    bpmLabel.box(FL_NO_BOX);
    bpmLabel.labelcolor(panelText);
    bpmLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    bpmInput.box(FL_FLAT_BOX);
    bpmInput.color(0x37415100);
    bpmInput.textcolor(panelText);
    bpmInput.cursor_color(panelText);
    bpmInput.when(FL_WHEN_ENTER_KEY);
    bpmInput.callback([](Fl_Widget*, void* d) {
        static_cast<LoopPanel*>(d)->commitBpm();
    }, this);
    bpmInput.onUnfocus([this]() { commitBpm(); });

    timeSigLabel.box(FL_NO_BOX);
    timeSigLabel.labelcolor(panelText);
    timeSigLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    timeSigSlash.box(FL_NO_BOX);
    timeSigSlash.labelcolor(panelText);
    timeSigSlash.align(FL_ALIGN_CENTER);

    timeSigTopInput.box(FL_FLAT_BOX);
    timeSigTopInput.color(0x37415100);
    timeSigTopInput.textcolor(panelText);
    timeSigTopInput.cursor_color(panelText);

    timeSigBotInput.box(FL_FLAT_BOX);
    timeSigBotInput.color(0x37415100);
    timeSigBotInput.textcolor(panelText);
    timeSigBotInput.cursor_color(panelText);

    end();
}

LoopPanel::~LoopPanel()
{
    swapObserver(timeline, nullptr, this);
}

void LoopPanel::setTimeline(ObservableTimeline* tl)
{
    swapObserver(timeline, tl, this);
    onTimelineChanged();
}

void LoopPanel::commitBpm()
{
    if (!timeline) return;
    float bpm = std::atof(bpmInput.value());
    if (bpm >= 20.0f && bpm <= 400.0f)
        timeline->setBpm(0, bpm);
    else
        onTimelineChanged();  // revert display to current value
}

void LoopPanel::onTimelineChanged()
{
    if (!timeline) return;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", timeline->bpmAt(0));
    bpmInput.value(buf);

    int top = 4, bot = 4;
    timeline->timeSigAt(0, top, bot);
    std::snprintf(buf, sizeof(buf), "%d", top);
    timeSigTopInput.value(buf);
    std::snprintf(buf, sizeof(buf), "%d", bot);
    timeSigBotInput.value(buf);

    redraw();
}

void LoopPanel::draw()
{
    fl_color(panelBorder);
    fl_rectf(x(), y(), w(), 1);
    fl_color(panelBg);
    fl_rectf(x(), y() + 1, w(), h() - 1);
    draw_children();
}

void LoopPanel::resize(int x, int y, int w, int h)
{
    int dx = x - this->x(), dy = y - this->y();
    Fl_Widget::resize(x, y, w, h);
    if (dx || dy)
        for (int i = 0; i < children(); i++) {
            Fl_Widget* c = child(i);
            c->position(c->x() + dx, c->y() + dy);
        }
}

// ======================================================
// LoopEditor button colours
// ======================================================

static constexpr Fl_Color btnInactiveBg  = 0x33415500;
static constexpr Fl_Color btnHoverBg     = 0x4B556300;
static constexpr Fl_Color btnActiveBg    = 0x3B82F600;
static constexpr Fl_Color btnActiveHover = 0x5B92FF00;
static constexpr Fl_Color btnBorder      = 0xCBD5E100;

// ======================================================
// LoopEditor
// ======================================================

LoopEditor::LoopEditor(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    box(FL_NO_BOX);
    panel = new LoopPanel(x, y + h - panelH, w, panelH);
    end();
}

LoopEditor::~LoopEditor()
{
    Fl::remove_timeout(timerCb, this);
    swapObserver(timeline, nullptr, this);
}

void LoopEditor::setTimeline(ObservableTimeline* tl)
{
    swapObserver(timeline, tl, this);
    panel->setTimeline(tl);
    onTimelineChanged();
}

void LoopEditor::setTransport(ITransport* t)
{
    Fl::remove_timeout(timerCb, this);
    transport = t;
    if (t) Fl::add_timeout(0.05, timerCb, this);
}

void LoopEditor::timerCb(void* data)
{
    auto* self = static_cast<LoopEditor*>(data);
    if (self->visible_r()) self->redraw();

    double interval = 0.1;
    if (self->transport && self->transport->isPlaying() && self->timeline) {
        float bpm = self->timeline->bpmAt((int)self->transport->position());
        // update fast enough that a beat never skips visually in a button
        interval = std::clamp(30.0 / bpm, 0.016, 0.05);
    }
    Fl::repeat_timeout(interval, timerCb, data);
}

bool LoopEditor::isEnabled(int trackIdx) const
{
    return trackIdx >= 0 && trackIdx < (int)toggled.size() && toggled[trackIdx];
}

float LoopEditor::beatProgress(int trackIdx) const
{
    if (!transport || !timeline) return 0.0f;
    const auto& tl = timeline->get();
    if (trackIdx < 0 || trackIdx >= (int)tl.tracks.size()) return 0.0f;

    int patId = tl.tracks[trackIdx].patternId;
    const Pattern* pat = nullptr;
    for (const auto& p : tl.patterns)
        if (p.id == patId) { pat = &p; break; }
    if (!pat || pat->lengthBeats <= 0.0f) return 0.0f;

    float bars = transport->position();
    int top, bottom;
    timeline->timeSigAt((int)bars, top, bottom);
    float globalBeats    = bars * (float)top;
    float posInPattern   = std::fmod(globalBeats, pat->lengthBeats);
    if (posInPattern < 0.0f) posInPattern += pat->lengthBeats;
    return posInPattern / pat->lengthBeats;
}

void LoopEditor::setContextPopup(TrackContextPopup* popup)
{
    contextPopup = popup;
}

void LoopEditor::onTimelineChanged()
{
    if (!timeline) return;
    int n = (int)timeline->get().tracks.size();
    if ((int)toggled.size() < n)
        toggled.resize(n, false);
    redraw();
}

void LoopEditor::btnRect(int idx, int& bx, int& by, int& bw, int& bh) const
{
    int available = w() - 2 * padX;
    bw = (available - (cols - 1) * btnGap) / cols;
    bh = btnH;
    bx = x() + padX + (idx % cols) * (bw + btnGap);
    by = y() + padY + (idx / cols) * (btnH + btnGap);
}

int LoopEditor::trackAt(int mx, int my) const
{
    if (!timeline) return -1;
    int n = (int)timeline->get().tracks.size();
    for (int i = 0; i < n; i++) {
        int bx, by, bw, bh;
        btnRect(i, bx, by, bw, bh);
        if (by >= y() + gridAreaH()) break;
        if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) return i;
    }
    return -1;
}

void LoopEditor::draw()
{
    // Grid area background
    fl_color(FL_WHITE);
    fl_rectf(x(), y(), w(), gridAreaH());

    fl_push_clip(x(), y(), w(), gridAreaH());

    if (timeline) {
        const auto& tracks = timeline->get().tracks;
        int n = (int)tracks.size();

        fl_font(FL_HELVETICA_BOLD, 14);
        for (int i = 0; i < n; i++) {
            int bx, by, bw, bh;
            btnRect(i, bx, by, bw, bh);
            if (by >= y() + gridAreaH()) break;

            bool isToggled = i < (int)toggled.size() && toggled[i];
            bool isHovered = i == hoveredIdx;

            Fl_Color bg = isToggled ? (isHovered ? btnActiveHover : btnActiveBg)
                                    : (isHovered ? btnHoverBg     : btnInactiveBg);
            fl_color(bg);
            fl_rectf(bx, by, bw, bh);

            fl_color(btnBorder);
            fl_rect(bx, by, bw, bh);

            fl_color(FL_WHITE);
            fl_draw(tracks[i].label.c_str(), bx + 4, by, bw - 8, bh,
                    FL_ALIGN_CENTER | FL_ALIGN_CLIP);

            // Playhead line – always running, regardless of toggle state
            float progress = beatProgress(i);
            int   lineX    = bx + (int)(progress * (bw - 1));
            fl_color(0xEF444400);
            fl_line_style(FL_SOLID, 2);
            fl_line(lineX, by + 4, lineX, by + bh - 4);
            fl_line_style(0);
        }
    }

    fl_pop_clip();
    draw_children();
}

int LoopEditor::handle(int event)
{
    if (Fl_Group::handle(event)) return 1;

    int mx = Fl::event_x(), my = Fl::event_y();

    switch (event) {
    case FL_ENTER:
        return 1;
    case FL_MOVE: {
        int idx = trackAt(mx, my);
        if (idx != hoveredIdx) { hoveredIdx = idx; redraw(); }
        return 1;
    }
    case FL_LEAVE:
        if (hoveredIdx != -1) { hoveredIdx = -1; redraw(); }
        return 1;
    case FL_PUSH: {
        int idx = trackAt(mx, my);
        if (idx < 0) return 0;
        take_focus();
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (idx >= (int)toggled.size()) toggled.resize(idx + 1, false);
            toggled[idx] = !toggled[idx];
            redraw();
            if (onToggleChanged) onToggleChanged();
            return 1;
        }
        if (Fl::event_button() == FL_RIGHT_MOUSE && contextPopup) {
            contextPopup->open(idx, timeline,
                               Fl::event_x_root(), Fl::event_y_root());
            return 1;
        }
        return 0;
    }
    default:
        return 0;
    }
}

void LoopEditor::resize(int x, int y, int w, int h)
{
    Fl_Group::resize(x, y, w, h);
    if (panel) panel->resize(x, y + h - panelH, w, panelH);
}
