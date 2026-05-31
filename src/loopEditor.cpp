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

void LoopPanel::setTimeline(ObservableSong* tl)
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
static constexpr Fl_Color headerBg       = 0x1E293B00;
static constexpr Fl_Color headerText     = 0xCBD5E100;
static constexpr Fl_Color emptyCell      = 0x1E293B00;

// ======================================================
// LoopEditor
// ======================================================

LoopEditor::LoopEditor(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    box(FL_NO_BOX);
    panel = new LoopPanel(x, y + h - panelH, w, panelH);

    axisToggleBtn = new ModernButton(0, 0, toggleBtnW, toggleBtnH, "Flip");
    axisToggleBtn->color(0x37415100);
    axisToggleBtn->labelcolor(headerText);
    axisToggleBtn->setBorderColor(btnBorder);
    axisToggleBtn->setBorderWidth(1);
    axisToggleBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<LoopEditor*>(d);
        self->tracksAsColumns = !self->tracksAsColumns;
        self->positionToggleBtn();
        self->redraw();
    }, this);

    end();
    positionToggleBtn();
}

LoopEditor::~LoopEditor()
{
    Fl::remove_timeout(timerCb, this);
    swapObserver(timeline, nullptr, this);
    swapActiveObserver(aps, nullptr, this);
}

void LoopEditor::setTimeline(ObservableSong* tl)
{
    swapObserver(timeline, tl, this);
    panel->setTimeline(tl);
    onTimelineChanged();
}

void LoopEditor::setActivePatterns(ActivePatternSet* a)
{
    swapActiveObserver(aps, a, this);
    redraw();
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

// ======================================================
// Grid helpers
// ======================================================

int LoopEditor::maxLanes() const
{
    if (!timeline) return 1;
    const auto& tracks = timeline->get().tracks;
    int mx = 1;
    for (const auto& t : tracks)
        mx = std::max(mx, (int)t.lanes.size());
    return mx;
}

int LoopEditor::numCols() const
{
    if (!timeline) return 1;
    return tracksAsColumns
        ? (int)timeline->get().tracks.size()
        : maxLanes();
}

int LoopEditor::numRows() const
{
    if (!timeline) return 1;
    return tracksAsColumns
        ? maxLanes()
        : (int)timeline->get().tracks.size();
}

int LoopEditor::topStripH() const
{
    // Mode A: track names are column headers → always show
    // Mode B: "P1/P2" labels are column headers → only if multiple pattern slots
    return tracksAsColumns ? trackHeaderH : (maxLanes() > 1 ? patLabelH : 0);
}

int LoopEditor::leftStripW() const
{
    // Mode A: "P1/P2" labels on left → only if multiple pattern slots
    // Mode B: track names on left → always show
    return tracksAsColumns ? (maxLanes() > 1 ? patLabelW : 0) : trackHeaderW;
}

void LoopEditor::btnRect(int col, int row, int& bx, int& by, int& bw, int& bh) const
{
    int nc = numCols();
    int nr = numRows();
    int lsw = leftStripW();
    int tsh = topStripH();

    int gx = x() + padX + lsw;
    int gy = y() + padY + tsh;

    int availW = w() - 2 * padX - lsw;
    int availH = gridAreaH() - 2 * padY - tsh;

    bw = nc > 0 ? (availW - (nc - 1) * btnGap) / nc : availW;
    int rawH = nr > 0 ? (availH - (nr - 1) * btnGap) / nr : availH;
    bh = std::max(40, std::min(btnH, rawH));

    bx = gx + col * (bw + btnGap);
    by = gy + row * (bh + btnGap);
}

bool LoopEditor::cellAt(int mx, int my, int& trackIdx, int& laneIdx) const
{
    if (!timeline) return false;
    const auto& tracks = timeline->get().tracks;
    int nc = numCols();
    int nr = numRows();
    for (int col = 0; col < nc; col++) {
        for (int row = 0; row < nr; row++) {
            int bx, by, bw, bh;
            btnRect(col, row, bx, by, bw, bh);
            if (by + bh > y() + gridAreaH()) continue;
            if (mx < bx || mx >= bx + bw) continue;
            if (my < by || my >= by + bh) continue;
            // Convert (col, row) → (trackIdx, laneIdx)
            int ti = tracksAsColumns ? col : row;
            int li = tracksAsColumns ? row : col;
            if (ti >= (int)tracks.size()) return false;
            if (li >= (int)tracks[ti].lanes.size()) return false;
            trackIdx = ti;
            laneIdx  = li;
            return true;
        }
    }
    return false;
}

void LoopEditor::positionToggleBtn()
{
    if (!axisToggleBtn) return;
    int tsh = topStripH();
    int ty  = y() + padY + (tsh > 0 ? (tsh - toggleBtnH) / 2 : 0);
    int tx  = x() + w() - padX - toggleBtnW;
    axisToggleBtn->resize(tx, ty, toggleBtnW, toggleBtnH);
}

// ======================================================
// State queries
// ======================================================

bool LoopEditor::isEnabled(int trackIdx, int laneIdx) const
{
    if (!timeline || !aps) return false;
    const auto& tracks = timeline->get().tracks;
    if (trackIdx < 0 || trackIdx >= (int)tracks.size()) return false;
    if (laneIdx < 0 || laneIdx >= (int)tracks[trackIdx].lanes.size()) return false;
    return aps->isPatternActive(tracks[trackIdx].lanes[laneIdx].patternId);
}

float LoopEditor::beatProgress(int trackIdx, int laneIdx) const
{
    if (!transport || !timeline || !aps) return 0.0f;
    const auto& tl = timeline->get();
    if (trackIdx < 0 || trackIdx >= (int)tl.tracks.size()) return 0.0f;
    const auto& track = tl.tracks[trackIdx];
    if (laneIdx < 0 || laneIdx >= (int)track.lanes.size()) return 0.0f;

    int patId = track.lanes[laneIdx].patternId;
    const Pattern* pat = nullptr;
    for (const auto& p : tl.patterns)
        if (p.id == patId) { pat = &p; break; }
    if (!pat || pat->lengthBeats <= 0.0f) return 0.0f;

    float bars = transport->position();
    int   top, bottom;
    timeline->timeSigAt((int)std::max(0.0f, bars), top, bottom);

    if (aps->isPatternActive(patId)) {
        float anchorBar    = aps->patternAnchorBar(patId);
        float elapsed      = (bars - anchorBar) * (float)top;
        float posInPattern = std::fmod(elapsed, pat->lengthBeats);
        if (posInPattern < 0.0f) posInPattern += pat->lengthBeats;
        return posInPattern / pat->lengthBeats;
    }

    // Virtual position: beat 0 will align with the next bar if enabled now.
    float nextBar      = std::floor(bars) + 1.0f;
    float beatsToNext  = (nextBar - bars) * (float)top;
    float virtualBeat  = std::fmod(pat->lengthBeats - beatsToNext, pat->lengthBeats);
    if (virtualBeat < 0.0f) virtualBeat += pat->lengthBeats;
    return virtualBeat / pat->lengthBeats;
}

void LoopEditor::setContextPopup(TrackContextPopup* popup)
{
    contextPopup = popup;
}

void LoopEditor::onTimelineChanged()
{
    positionToggleBtn();
    redraw();
}

void LoopEditor::onActivePatternsChanged()
{
    redraw();
}

// ======================================================
// Draw
// ======================================================

void LoopEditor::draw()
{
    // Grid area background
    fl_color(0x0F172A00);  // dark navy
    fl_rectf(x(), y(), w(), gridAreaH());

    fl_push_clip(x(), y(), w(), gridAreaH());

    if (timeline) {
        const auto& tl     = timeline->get();
        const auto& tracks = tl.tracks;
        int nc   = numCols();
        int nr   = numRows();
        int lsw  = leftStripW();
        int tsh  = topStripH();

        // ---- Column headers (top strip) ----
        if (tsh > 0) {
            fl_color(headerBg);
            fl_rectf(x() + padX + lsw, y() + padY, w() - 2 * padX - lsw - toggleBtnW - 4, tsh);

            fl_font(FL_HELVETICA_BOLD, 11);
            fl_color(headerText);

            for (int col = 0; col < nc; col++) {
                int bx, by, bw, bh;
                btnRect(col, 0, bx, by, bw, bh);
                int hx = bx, hy = y() + padY, hw = bw, hh = tsh;

                const char* label = nullptr;
                char buf[16];
                std::string nameStr;
                if (tracksAsColumns) {
                    // track name
                    int ti = col;
                    if (ti < (int)tracks.size())
                        nameStr = tl.instrumentName(tracks[ti].instrumentId);
                    label = nameStr.empty() ? nullptr : nameStr.c_str();
                } else {
                    // "P1", "P2", etc.
                    std::snprintf(buf, sizeof(buf), "P%d", col + 1);
                    label = buf;
                }
                if (label)
                    fl_draw(label, hx, hy, hw, hh, FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            }
        }

        // ---- Row labels (left strip) ----
        if (lsw > 0) {
            fl_color(headerBg);
            fl_rectf(x() + padX, y() + padY + tsh, lsw - 2, gridAreaH() - 2 * padY - tsh);

            fl_font(FL_HELVETICA_BOLD, 11);
            fl_color(headerText);

            for (int row = 0; row < nr; row++) {
                int bx, by, bw, bh;
                btnRect(0, row, bx, by, bw, bh);
                int lx = x() + padX, ly = by, lw = lsw - 2, lh = bh;

                const char* label = nullptr;
                char buf[16];
                std::string nameStr;
                if (tracksAsColumns) {
                    // "P1", "P2", etc.
                    std::snprintf(buf, sizeof(buf), "P%d", row + 1);
                    label = buf;
                } else {
                    // track name
                    int ti = row;
                    if (ti < (int)tracks.size())
                        nameStr = tl.instrumentName(tracks[ti].instrumentId);
                    label = nameStr.empty() ? nullptr : nameStr.c_str();
                }
                if (label)
                    fl_draw(label, lx, ly, lw, lh, FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            }
        }

        // ---- Grid cells ----
        fl_font(FL_HELVETICA_BOLD, 13);

        for (int col = 0; col < nc; col++) {
            for (int row = 0; row < nr; row++) {
                int bx, by, bw, bh;
                btnRect(col, row, bx, by, bw, bh);
                if (by + bh > y() + gridAreaH()) continue;

                int ti = tracksAsColumns ? col : row;
                int li = tracksAsColumns ? row : col;

                // Empty cell — track has no lane at this index
                if (ti >= (int)tracks.size() || li >= (int)tracks[ti].lanes.size()) {
                    fl_color(emptyCell);
                    fl_rectf(bx, by, bw, bh);
                    fl_color(btnBorder);
                    fl_line_style(FL_DASH, 1);
                    fl_rect(bx, by, bw, bh);
                    fl_line_style(0);
                    continue;
                }

                int patId    = tracks[ti].lanes[li].patternId;
                bool isOn    = aps && aps->isPatternActive(patId);
                bool isHov   = (col == hoveredCol && row == hoveredRow);

                Fl_Color bg = isOn ? (isHov ? btnActiveHover : btnActiveBg)
                                   : (isHov ? btnHoverBg     : btnInactiveBg);
                fl_color(bg);
                fl_rectf(bx, by, bw, bh);

                fl_color(btnBorder);
                fl_line_style(FL_SOLID, 1);
                fl_rect(bx, by, bw, bh);
                fl_line_style(0);

                // Pattern name
                const char* patName = nullptr;
                for (const auto& p : tl.patterns)
                    if (p.id == patId) { patName = p.name.c_str(); break; }
                if (patName) {
                    fl_color(FL_WHITE);
                    fl_draw(patName, bx + 4, by, bw - 8, bh,
                            FL_ALIGN_CENTER | FL_ALIGN_CLIP);
                }

                // Playhead line — always running, regardless of toggle state
                float progress = beatProgress(ti, li);
                int   lineX    = bx + (int)(progress * (bw - 1));
                fl_color(isOn ? 0xEF444400 : 0xFFFFFF40);
                fl_line_style(FL_SOLID, 2);
                fl_line(lineX, by + 4, lineX, by + bh - 4);
                fl_line_style(0);
            }
        }
    }

    fl_pop_clip();
    draw_children();
}

// ======================================================
// Handle
// ======================================================

int LoopEditor::handle(int event)
{
    if (Fl_Group::handle(event)) return 1;

    int mx = Fl::event_x(), my = Fl::event_y();

    switch (event) {
    case FL_ENTER:
        return 1;
    case FL_MOVE: {
        int ti = -1, li = -1;
        int newCol = -1, newRow = -1;
        if (cellAt(mx, my, ti, li)) {
            newCol = tracksAsColumns ? ti : li;
            newRow = tracksAsColumns ? li : ti;
        }
        if (newCol != hoveredCol || newRow != hoveredRow) {
            hoveredCol = newCol;
            hoveredRow = newRow;
            redraw();
        }
        return 1;
    }
    case FL_LEAVE:
        if (hoveredCol != -1 || hoveredRow != -1) {
            hoveredCol = hoveredRow = -1;
            redraw();
        }
        return 1;
    case FL_PUSH: {
        int trackIdx = -1, laneIdx = -1;
        if (!cellAt(mx, my, trackIdx, laneIdx)) return 0;
        take_focus();

        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (timeline && aps) {
                const auto& tracks = timeline->get().tracks;
                if (trackIdx < (int)tracks.size() && laneIdx < (int)tracks[trackIdx].lanes.size()) {
                    int   patId = tracks[trackIdx].lanes[laneIdx].patternId;
                    float bar   = transport ? transport->position() : 0.0f;
                    if (aps->isPatternActive(patId)) {
                        aps->deactivate(patId);
                    } else {
                        float anchorBar = bar;
                        if (transport && transport->isPlaying()) {
                            const Pattern* pat = nullptr;
                            for (const auto& p : timeline->get().patterns)
                                if (p.id == patId) { pat = &p; break; }
                            if (pat && pat->lengthBeats > 0.0f) {
                                int top, bottom;
                                timeline->timeSigAt((int)std::max(0.0f, bar), top, bottom);
                                float beatsToNext = (std::floor(bar) + 1.0f - bar) * (float)top;
                                float virtualBeat = std::fmod(pat->lengthBeats - beatsToNext, pat->lengthBeats);
                                if (virtualBeat < 0.0f) virtualBeat += pat->lengthBeats;
                                anchorBar = bar - virtualBeat / (float)top;
                            }
                        }
                        aps->activate(patId, anchorBar);
                    }
                }
            }
            redraw();
            if (onToggleChanged) onToggleChanged();
            return 1;
        }
        if (Fl::event_button() == FL_RIGHT_MOUSE && contextPopup && patternObs) {
            const auto& tracks = patternObs->get().tracks;
            int trackId = trackIdx < (int)tracks.size() ? tracks[trackIdx].id : -1;
            int laneId  = -1;
            if (trackIdx < (int)tracks.size() && laneIdx < (int)tracks[trackIdx].lanes.size())
                laneId = tracks[trackIdx].lanes[laneIdx].id;
            contextPopup->open(trackId, laneId, patternObs,
                               Fl::event_x_root(), Fl::event_y_root());
            return 1;
        }
        return 0;
    }
    default:
        return 0;
    }
}

// ======================================================
// Resize
// ======================================================

void LoopEditor::resize(int x, int y, int w, int h)
{
    Fl_Group::resize(x, y, w, h);
    if (panel) panel->resize(x, y + h - panelH, w, panelH);
    positionToggleBtn();
}
