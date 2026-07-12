#include "loopEditor.hpp"
#include "panelStyle.hpp"
#include "timeSettings.hpp"
#include "appWindow.hpp"
#include "cursors.hpp"
#include "inlineEditDispatch.hpp"
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
static constexpr int lpBpmW     = 70;
static constexpr int lpSmallW   = 36;
static constexpr int lpChoiceW  = 50;
static constexpr int lpSlashW   = 14;
static constexpr int lpSmGap    = 4;
static constexpr int lpGroupGap = 20;

static int lpCtrlY(int y, int h) { return y + (h - lpCtrlH) / 2; }
static int lpBpmLabelX(int x)   { return x + lpPad; }
static int lpBpmInputX(int x)   { return lpBpmLabelX(x) + lpLabelW + lpSmGap; }
static int lpTsLabelX(int x)    { return lpBpmInputX(x) + lpBpmW + lpGroupGap; }
static int lpTsNumX(int x)      { return lpTsLabelX(x) + lpLabelW + lpSmGap; }
static int lpTsSlashX(int x)    { return lpTsNumX(x) + lpSmallW; }
static int lpTsDenX(int x)      { return lpTsSlashX(x) + lpSlashW; }

// ======================================================
// LoopPanel
// ======================================================

LoopPanel::LoopPanel(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      bpmLabel    (lpBpmLabelX(x), lpCtrlY(y, h), lpLabelW,  lpCtrlH, "BPM"),
      bpmInput    (lpBpmInputX(x), lpCtrlY(y, h), lpBpmW,    lpCtrlH),
      timeSigLabel(lpTsLabelX(x),  lpCtrlY(y, h), lpLabelW,  lpCtrlH, "Sig"),
      timeSigNum  (lpTsNumX(x),    lpCtrlY(y, h), lpSmallW,  lpCtrlH),
      timeSigSlash(lpTsSlashX(x),  lpCtrlY(y, h), lpSlashW,  lpCtrlH, "/"),
      timeSigDen  (lpTsDenX(x),    lpCtrlY(y, h), lpChoiceW, lpCtrlH)
{
    box(FL_NO_BOX);

    bpmLabel.box(FL_NO_BOX);
    bpmLabel.labelcolor(panelText);
    bpmLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    bpmInput.step(1.0);              // set before type(): step() flips the input
    bpmInput.type(FL_FLOAT_INPUT);   // numeric type back to integer when whole
    bpmInput.format("%.1f");
    bpmInput.range(timeSettings::bpmMin, timeSettings::bpmMax);
    bpmInput.wrap(0);
    bpmInput.when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
    bpmInput.callback([](Fl_Widget*, void* d) {
        static_cast<LoopPanel*>(d)->commitBpm();
    }, this);

    timeSigLabel.box(FL_NO_BOX);
    timeSigLabel.labelcolor(panelText);
    timeSigLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    timeSigSlash.box(FL_NO_BOX);
    timeSigSlash.labelcolor(panelText);
    timeSigSlash.align(FL_ALIGN_CENTER);

    auto commitCb = [](Fl_Widget*, void* d) {
        static_cast<LoopPanel*>(d)->commitTimeSig();
    };

    timeSigNum.color(0x37415100);
    timeSigNum.setBorderColor(panelCtrlBorder);
    timeSigNum.textcolor(panelText);
    timeSigNum.cursor_color(panelText);
    timeSigNum.labelcolor(panelText);
    timeSigNum.range(timeSettings::numeratorMin, timeSettings::numeratorMax);
    timeSigNum.step(1);
    timeSigNum.value(timeSettings::numeratorDefault);
    timeSigNum.when(FL_WHEN_RELEASE);
    timeSigNum.callback(commitCb, this);

    timeSigDen.color(0x37415100);
    timeSigDen.labelcolor(panelText);
    timeSigDen.setBorderColor(panelCtrlBorder);
    for (const char* v : timeSettings::denominatorLabels)
        timeSigDen.add(v);
    timeSigDen.value(timeSettings::denominatorDefaultIndex);
    timeSigDen.callback(commitCb, this);

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
    // The spinner already clamps to its [20, 400] range on every change.
    // ObservableSong::setBpm re-anchors the transport so the playhead keeps its
    // musical position across the tempo change instead of scrubbing.
    timeline->setBpm(0, (float)bpmInput.value());
}

void LoopPanel::commitTimeSig()
{
    if (!timeline) return;
    // The spinner clamps the numerator to its range; the dropdown only ever
    // offers the denominators timeSettings allows.
    timeline->setTimeSig(0, (int)timeSigNum.value(),
                         timeSettings::denominatorAt(timeSigDen.value()));
}

void LoopPanel::onTimelineChanged()
{
    if (!timeline) return;

    bpmInput.value(timeline->bpmAt(0));

    int top = timeSettings::numeratorDefault;
    int bot = timeSettings::denominatorDefault;
    timeline->timeSigAt(0, top, bot);
    timeSigNum.value(top);
    timeSigDen.value(timeSettings::denominatorIndex(bot));

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

// ======================================================
// LoopEditor
// ======================================================

LoopEditor::LoopEditor(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      nameInput(x, y, trackHeaderW, trackHeaderH)
{
    box(FL_NO_BOX);

    nameInput.hide();
    nameInput.box(FL_FLAT_BOX);
    nameInput.color(0x37415100);
    nameInput.textcolor(headerText);
    nameInput.cursor_color(headerText);
    nameInput.when(FL_WHEN_ENTER_KEY);
    nameInput.callback([](Fl_Widget*, void* d) {
        static_cast<LoopEditor*>(d)->commitInstrumentEdit();
    }, this);
    nameInput.onUnfocus([this]() { commitInstrumentEdit(); });

    panel = new LoopPanel(x, y + h - panelH, w, panelH);

    axisToggleBtn = new ModernButton(0, 0, toggleBtnW, toggleBtnH, "Flip");
    axisToggleBtn->color(0x37415100);
    axisToggleBtn->labelcolor(headerText);
    axisToggleBtn->setBorderColor(btnBorder);
    axisToggleBtn->setBorderWidth(1);
    axisToggleBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<LoopEditor*>(d);
        self->tracksAsColumns = !self->tracksAsColumns;
        self->scrollX = self->scrollY = 0;
        self->positionToggleBtn();
        self->updateScrollbars();
        self->redraw();
    }, this);

    hScroll = new GridScrollPane(x, y, scrollW, scrollW, GridScrollPane::HORIZONTAL);
    hScroll->linesize(cellW + btnGap);
    hScroll->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<LoopEditor*>(d);
        self->scrollX = static_cast<GridScrollPane*>(w)->value();
        self->redraw();
    }, this);
    hScroll->hide();

    vScroll = new GridScrollPane(x, y, scrollW, scrollW, GridScrollPane::VERTICAL);
    vScroll->linesize(cellH + btnGap);
    vScroll->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<LoopEditor*>(d);
        self->scrollY = static_cast<GridScrollPane*>(w)->value();
        self->redraw();
    }, this);
    vScroll->hide();

    end();
    positionToggleBtn();
}

LoopEditor::~LoopEditor()
{
    Fl::remove_timeout(timerCb, this);
    swapObserver(timeline, nullptr, this);
    swapLoopObserver(loopMgr, nullptr, this);
}

void LoopEditor::setTimeline(ObservableSong* tl)
{
    swapObserver(timeline, tl, this);
    panel->setTimeline(tl);
    onTimelineChanged();
}

void LoopEditor::setLoopManager(LoopManager* a)
{
    swapLoopObserver(loopMgr, a, this);
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

int LoopEditor::trackForAxis(int axisIdx) const
{
    if (!timeline) return -1;
    const auto& lo = timeline->get().loopOrder;
    if (axisIdx < 0 || axisIdx >= (int)lo.size()) return -1;
    return timeline->trackIndexForId(lo[axisIdx]);
}

int LoopEditor::laneForSlot(int trackVecIdx, int slot) const
{
    if (!timeline) return -1;
    const auto& tracks = timeline->get().tracks;
    if (trackVecIdx < 0 || trackVecIdx >= (int)tracks.size()) return -1;
    const auto& t = tracks[trackVecIdx];
    if (slot < 0 || slot >= (int)t.loopLanes.size()) return -1;
    int laneId = t.loopLanes[slot];
    for (int i = 0; i < (int)t.lanes.size(); i++)
        if (t.lanes[i].id == laneId) return i;
    return -1;
}

void LoopEditor::btnRect(int col, int row, int& bx, int& by, int& bw, int& bh) const
{
    int gx = x() + padX + leftStripW();
    int gy = y() + padY + topStripH();

    bw = cellW;
    bh = cellH;
    bx = gx + col * (cellW + btnGap) - scrollX;
    by = gy + row * (cellH + btnGap) - scrollY;
}

LoopEditor::Layout LoopEditor::computeLayout() const
{
    Layout L{};
    int nc  = numCols();
    int nr  = numRows();
    int lsw = leftStripW();
    int tsh = topStripH();

    L.contentW = nc > 0 ? nc * cellW + (nc - 1) * btnGap : 0;
    L.contentH = nr > 0 ? nr * cellH + (nr - 1) * btnGap : 0;

    int areaX = x() + padX + lsw;
    int areaY = y() + padY + tsh;
    int areaW = w() - 2 * padX - lsw;
    int areaH = gridAreaH() - 2 * padY - tsh;

    // Each scrollbar steals space from the other axis, so resolve mutually.
    bool needV = L.contentH > areaH;
    bool needH = L.contentW > areaW;
    if (needV && !needH) needH = L.contentW > (areaW - scrollW);
    if (needH && !needV) needV = L.contentH > (areaH - scrollW);

    L.needV = needV;
    L.needH = needH;
    L.vpX = areaX;
    L.vpY = areaY;
    L.vpW = areaW - (needV ? scrollW : 0);
    L.vpH = areaH - (needH ? scrollW : 0);
    L.maxScrollX = std::max(0, L.contentW - L.vpW);
    L.maxScrollY = std::max(0, L.contentH - L.vpH);
    return L;
}

void LoopEditor::updateScrollbars()
{
    if (!hScroll || !vScroll) return;
    Layout L = computeLayout();

    scrollX = std::clamp(scrollX, 0, L.maxScrollX);
    scrollY = std::clamp(scrollY, 0, L.maxScrollY);

    if (L.needH) {
        hScroll->resize(L.vpX, L.vpY + L.vpH, L.vpW, scrollW);
        hScroll->value(scrollX, L.vpW, 0, L.contentW);
        hScroll->show();
    } else {
        hScroll->hide();
    }

    if (L.needV) {
        vScroll->resize(L.vpX + L.vpW, L.vpY, scrollW, L.vpH);
        vScroll->value(scrollY, L.vpH, 0, L.contentH);
        vScroll->show();
    } else {
        vScroll->hide();
    }
}

bool LoopEditor::cellAt(int mx, int my, int& trackIdx, int& laneIdx, int& col, int& row) const
{
    if (!timeline) return false;
    Layout L = computeLayout();
    if (mx < L.vpX || mx >= L.vpX + L.vpW || my < L.vpY || my >= L.vpY + L.vpH)
        return false;
    int nc = numCols();
    int nr = numRows();
    for (int c = 0; c < nc; c++) {
        for (int r = 0; r < nr; r++) {
            int bx, by, bw, bh;
            btnRect(c, r, bx, by, bw, bh);
            if (mx < bx || mx >= bx + bw) continue;
            if (my < by || my >= by + bh) continue;
            // Convert (col, row) → real (trackIdx, laneIdx)
            int ti = trackForAxis(tracksAsColumns ? c : r);
            int li = laneForSlot(ti, tracksAsColumns ? r : c);
            if (ti < 0 || li < 0) return false;
            trackIdx = ti;
            laneIdx  = li;
            col      = c;
            row      = r;
            return true;
        }
    }
    return false;
}

bool LoopEditor::instrLabelAt(int mx, int my, int& axisIdx) const
{
    if (!timeline) return false;
    Layout L = computeLayout();
    // Instrument names live in the top strip (mode A) or left strip (mode B).
    if (tracksAsColumns) {
        if (my < y() + padY || my >= y() + padY + topStripH()) return false;
        if (mx < L.vpX || mx >= L.vpX + L.vpW) return false;
    } else {
        if (mx < x() + padX || mx >= x() + padX + leftStripW()) return false;
        if (my < L.vpY || my >= L.vpY + L.vpH) return false;
    }
    int n = numCols();  // instrument axis is columns in mode A, rows in mode B
    int axisCount = tracksAsColumns ? n : numRows();
    for (int a = 0; a < axisCount; a++) {
        int bx, by, bw, bh;
        if (tracksAsColumns) btnRect(a, 0, bx, by, bw, bh);
        else                 btnRect(0, a, bx, by, bw, bh);
        if (tracksAsColumns) { if (mx >= bx && mx < bx + bw) { axisIdx = a; return true; } }
        else                 { if (my >= by && my < by + bh) { axisIdx = a; return true; } }
    }
    return false;
}

bool LoopEditor::instrLabelRect(int axisIdx, int& lx, int& ly, int& lw, int& lh) const
{
    if (!timeline) return false;
    int axisCount = tracksAsColumns ? numCols() : numRows();
    if (axisIdx < 0 || axisIdx >= axisCount) return false;
    int bx, by, bw, bh;
    if (tracksAsColumns) {
        btnRect(axisIdx, 0, bx, by, bw, bh);
        lx = bx; ly = y() + padY; lw = bw; lh = topStripH();
    } else {
        btnRect(0, axisIdx, bx, by, bw, bh);
        lx = x() + padX; ly = by; lw = leftStripW() - 2; lh = bh;
    }
    return true;
}

void LoopEditor::startInstrumentEdit(int axisIdx)
{
    if (!timeline) return;
    int ti = trackForAxis(axisIdx);
    if (ti < 0) return;
    int lx, ly, lw, lh;
    if (!instrLabelRect(axisIdx, lx, ly, lw, lh)) return;

    editingAxisIdx = axisIdx;
    editingInstrId = timeline->get().tracks[ti].instrumentId;
    originalName   = timeline->get().instrumentName(editingInstrId);

    nameInput.resize(lx, ly, lw, lh);
    nameInput.value(originalName.c_str());
    nameInput.textcolor(headerText);
    nameInput.show();
    nameInput.take_focus();
    nameInput.position(nameInput.size(), 0);
    nameInput.onChange([this]() { checkDuplicateName(); });
    InlineEditDispatch::install(this, [this]() { commitInstrumentEdit(); });
    redraw();
}

void LoopEditor::checkDuplicateName()
{
    if (!timeline || editingInstrId < 0) return;
    std::string cur = nameInput.value();
    bool dup = false;
    for (const auto& instr : timeline->get().instruments)
        if (instr.id != editingInstrId && instr.name == cur) { dup = true; break; }
    nameInput.textcolor(dup ? FL_RED : headerText);
    nameInput.redraw();
}

void LoopEditor::commitInstrumentEdit()
{
    if (editingInstrId < 0) return;
    int instrId    = editingInstrId;
    editingInstrId = -1;
    editingAxisIdx = -1;
    InlineEditDispatch::uninstall();
    std::string newName = nameInput.value();
    nameInput.hide();
    if (timeline) {
        // A duplicate name reverts to the original, matching the Song Editor.
        bool dup = false;
        for (const auto& instr : timeline->get().instruments)
            if (instr.id != instrId && instr.name == newName) { dup = true; break; }
        timeline->renameInstrument(instrId, dup ? originalName : newName);
    }
    redraw();
}

void LoopEditor::cancelInstrumentEdit()
{
    if (editingInstrId < 0) return;
    editingInstrId = -1;
    editingAxisIdx = -1;
    InlineEditDispatch::uninstall();
    nameInput.hide();
    redraw();
}

int LoopEditor::computeDropGap(int mx, int my) const
{
    int numTracks = timeline ? (int)timeline->get().loopOrder.size() : 0;
    if (tracksAsColumns) {
        int gx     = x() + padX + leftStripW();
        int stride = cellW + btnGap;
        int gap    = (int)std::lround((double)(mx - gx + scrollX) / stride);
        return std::clamp(gap, 0, numTracks);
    } else {
        int gy     = y() + padY + topStripH();
        int stride = cellH + btnGap;
        int gap    = (int)std::lround((double)(my - gy + scrollY) / stride);
        return std::clamp(gap, 0, numTracks);
    }
}

void LoopEditor::computePatternDrop(int mx, int my)
{
    dropTrackIdx  = -1;
    dropSlot      = -1;
    dropForbidden = true;
    if (!timeline) return;

    int gx = x() + padX + leftStripW();
    int gy = y() + padY + topStripH();
    int numInstr = (int)timeline->get().loopOrder.size();

    // Track axis = columns (mode A) / rows (mode B); lane axis is the other one.
    int axisTrack, slot;
    if (tracksAsColumns) {
        axisTrack = (int)std::floor((double)(mx - gx + scrollX) / (cellW + btnGap));
        slot      = (int)std::lround((double)(my - gy + scrollY) / (cellH + btnGap));
    } else {
        axisTrack = (int)std::floor((double)(my - gy + scrollY) / (cellH + btnGap));
        slot      = (int)std::lround((double)(mx - gx + scrollX) / (cellW + btnGap));
    }
    axisTrack = std::clamp(axisTrack, 0, std::max(0, numInstr - 1));

    int ti = trackForAxis(axisTrack);
    if (ti < 0) return;
    const auto& dt = timeline->get().tracks[ti];
    dropTrackIdx  = ti;
    dropSlot      = std::clamp(slot, 0, (int)dt.loopLanes.size());
    dropForbidden = !timeline->canMoveLaneToTrack(dragLaneId, dt.id);
}

void LoopEditor::togglePattern(int trackIdx, int laneIdx)
{
    if (!timeline || !loopMgr) return;
    const auto& tracks = timeline->get().tracks;
    if (trackIdx < 0 || trackIdx >= (int)tracks.size()) return;
    if (laneIdx < 0 || laneIdx >= (int)tracks[trackIdx].lanes.size()) return;

    int   patId = tracks[trackIdx].lanes[laneIdx].patternId;
    float bar   = transport ? transport->position() : 0.0f;
    if (loopMgr->isPatternActive(patId)) {
        loopMgr->deactivate(patId);
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
        loopMgr->activate(patId, anchorBar);
    }
    redraw();
    if (onToggleChanged) onToggleChanged();
}

void LoopEditor::positionToggleBtn()
{
    if (!axisToggleBtn) return;
    // Sit in the control bar at the bottom, on the right-hand side,
    // vertically centred to line up with the BPM/time-signature controls.
    int panelTop = y() + h() - panelH;
    int ty = panelTop + (panelH - toggleBtnH) / 2;
    int tx = x() + w() - lpPad - toggleBtnW;
    axisToggleBtn->resize(tx, ty, toggleBtnW, toggleBtnH);
}

// ======================================================
// State queries
// ======================================================

bool LoopEditor::isEnabled(int trackIdx, int laneIdx) const
{
    if (!timeline || !loopMgr) return false;
    const auto& tracks = timeline->get().tracks;
    if (trackIdx < 0 || trackIdx >= (int)tracks.size()) return false;
    if (laneIdx < 0 || laneIdx >= (int)tracks[trackIdx].lanes.size()) return false;
    return loopMgr->isPatternActive(tracks[trackIdx].lanes[laneIdx].patternId);
}

float LoopEditor::beatProgress(int trackIdx, int laneIdx) const
{
    if (!transport || !timeline || !loopMgr) return 0.0f;
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

    if (loopMgr->isPatternActive(patId)) {
        float anchorBar    = loopMgr->patternAnchorBar(patId);
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

void LoopEditor::setContextPopup(LoopContextPopup* popup)
{
    contextPopup = popup;
}

void LoopEditor::onTimelineChanged()
{
    positionToggleBtn();
    updateScrollbars();
    redraw();
}

void LoopEditor::onLoopsChanged()
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

    Layout L = computeLayout();

    if (timeline) {
        const auto& tl     = timeline->get();
        const auto& tracks = tl.tracks;
        int nc   = numCols();
        int nr   = numRows();
        int lsw  = leftStripW();
        int tsh  = topStripH();

        // ---- Column headers (top strip) — frozen vertically, scroll horizontally ----
        if (tsh > 0) {
            fl_push_clip(L.vpX, y() + padY, L.vpW, tsh);
            fl_color(headerBg);
            fl_rectf(L.vpX, y() + padY, L.vpW, tsh);

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
                    int ti = trackForAxis(col);
                    if (ti >= 0)
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
            fl_pop_clip();
        }

        // ---- Row labels (left strip) — frozen horizontally, scroll vertically ----
        if (lsw > 0) {
            fl_push_clip(x() + padX, L.vpY, lsw, L.vpH);
            fl_color(headerBg);
            fl_rectf(x() + padX, L.vpY, lsw - 2, L.vpH);

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
                    int ti = trackForAxis(row);
                    if (ti >= 0)
                        nameStr = tl.instrumentName(tracks[ti].instrumentId);
                    label = nameStr.empty() ? nullptr : nameStr.c_str();
                }
                if (label)
                    fl_draw(label, lx, ly, lw, lh, FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            }
            fl_pop_clip();
        }

        // ---- Grid cells — clipped to the scrollable viewport ----
        fl_push_clip(L.vpX, L.vpY, L.vpW, L.vpH);
        fl_font(FL_HELVETICA_BOLD, 13);

        for (int col = 0; col < nc; col++) {
            for (int row = 0; row < nr; row++) {
                int bx, by, bw, bh;
                btnRect(col, row, bx, by, bw, bh);

                int ti   = trackForAxis(tracksAsColumns ? col : row);
                int slot = tracksAsColumns ? row : col;
                int li   = laneForSlot(ti, slot);

                // Empty cell — track has no lane at this slot; draw nothing.
                if (ti < 0 || li < 0)
                    continue;

                int patId    = tracks[ti].lanes[li].patternId;
                bool isOn    = loopMgr && loopMgr->isPatternActive(patId);
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
        fl_pop_clip();

        // ---- Drop indicator while reordering instruments ----
        if (draggingInstr && dropGap >= 0) {
            fl_color(0xFBBF2400);  // amber
            fl_line_style(FL_SOLID, 2);
            if (tracksAsColumns) {
                int gx = x() + padX + leftStripW();
                int lineX = gx + dropGap * (cellW + btnGap) - scrollX - btnGap / 2;
                lineX = std::clamp(lineX, L.vpX, L.vpX + L.vpW);
                fl_line(lineX, y() + padY, lineX, L.vpY + L.vpH);
            } else {
                int gy = y() + padY + topStripH();
                int lineY = gy + dropGap * (cellH + btnGap) - scrollY - btnGap / 2;
                lineY = std::clamp(lineY, L.vpY, L.vpY + L.vpH);
                fl_line(x() + padX, lineY, L.vpX + L.vpW, lineY);
            }
            fl_line_style(0);
        }

        // ---- Drop indicator while moving a pattern (lane) ----
        if (patternDragging && !dropForbidden && dropTrackIdx >= 0) {
            // Axis position of the destination instrument.
            int destAxis = -1;
            for (int a = 0; a < (int)tl.loopOrder.size(); a++)
                if (trackForAxis(a) == dropTrackIdx) { destAxis = a; break; }
            if (destAxis >= 0) {
                fl_push_clip(L.vpX, L.vpY, L.vpW, L.vpH);
                fl_color(0xFBBF2400);  // amber
                fl_line_style(FL_SOLID, 2);
                if (tracksAsColumns) {
                    int bx, by, bw, bh;
                    btnRect(destAxis, 0, bx, by, bw, bh);
                    int gy = y() + padY + topStripH();
                    int lineY = gy + dropSlot * (cellH + btnGap) - scrollY - btnGap / 2;
                    fl_line(bx, lineY, bx + bw, lineY);
                } else {
                    int bx, by, bw, bh;
                    btnRect(0, destAxis, bx, by, bw, bh);
                    int gx = x() + padX + leftStripW();
                    int lineX = gx + dropSlot * (cellW + btnGap) - scrollX - btnGap / 2;
                    fl_line(lineX, by, lineX, by + bh);
                }
                fl_line_style(0);
                fl_pop_clip();
            }
        }
    }

    draw_children();
}

// ======================================================
// Handle
// ======================================================

static constexpr int dragThreshold = 4;  // px before an instrument drag begins

int LoopEditor::handle(int event)
{
    // Escape abandons an in-progress rename (the focused input ignores it, so it
    // bubbles up to here).
    if (event == FL_KEYBOARD && editingInstrId >= 0 && Fl::event_key() == FL_Escape) {
        cancelInstrumentEdit();
        return 1;
    }

    // While an instrument-reorder drag is in progress, own all drag/release
    // events ourselves rather than letting a child (e.g. a scrollbar passed
    // under the cursor) swallow the commit.
    bool ownDrag = (dragAxisFrom >= 0 || dragLaneId >= 0)
                && (event == FL_DRAG || event == FL_RELEASE);

    // Let child widgets (scrollbars, Flip button) handle events first. This
    // also drives their enter/leave + hover state for FL_MOVE. But
    // Fl_Group::handle() ALWAYS returns 1 for FL_MOVE (it claims belowmouse),
    // so we must not let that short-circuit our own hover/cursor handling over
    // the drawn grid cells — fall through to the switch for FL_MOVE.
    if (!ownDrag) {
        bool childHandled = Fl_Group::handle(event);
        if (event != FL_MOVE && childHandled) return 1;
    }

    int mx = Fl::event_x(), my = Fl::event_y();

    switch (event) {
    case FL_ENTER:
        return 1;
    case FL_MOVE: {
        int ti = -1, li = -1, newCol = -1, newRow = -1;
        bool overCell = cellAt(mx, my, ti, li, newCol, newRow);
        int axisIdx = -1;
        bool overLabel = instrLabelAt(mx, my, axisIdx);
        if (window()) {
            // Labels and cells both get the context cursor; the move cursor is
            // shown only while actually dragging an instrument (see FL_DRAG).
            if (overLabel || overCell) window()->cursor(contextMenuCursorImage(), 0, 0);
            else                       window()->cursor(FL_CURSOR_DEFAULT);
        }
        if (newCol != hoveredCol || newRow != hoveredRow) {
            hoveredCol = newCol;
            hoveredRow = newRow;
            redraw();
        }
        return 1;
    }
    case FL_LEAVE:
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        if (hoveredCol != -1 || hoveredRow != -1) {
            hoveredCol = hoveredRow = -1;
            redraw();
        }
        return 1;
    case FL_PUSH: {
        int labelAxis = -1;
        bool overLabel = instrLabelAt(mx, my, labelAxis);

        // Double-click an instrument name → inline rename.
        if (Fl::event_button() == FL_LEFT_MOUSE && timeline && overLabel
            && Fl::event_clicks() > 0) {
            startInstrumentEdit(labelAxis);
            return 1;
        }
        // Right-click an instrument label → the same context menu as a pattern
        // cell, but for the whole instrument (no specific pattern).
        if (Fl::event_button() == FL_RIGHT_MOUSE && overLabel
            && contextPopup && patternObs && timeline) {
            int ti = trackForAxis(labelAxis);
            if (ti >= 0) {
                int trackId = timeline->get().tracks[ti].id;
                contextPopup->open(trackId, -1, patternObs,
                                   Fl::event_x(), Fl::event_y(), true);
            }
            return 1;
        }
        // Any other press commits an in-progress rename before doing its thing.
        if (editingInstrId >= 0)
            commitInstrumentEdit();

        // Begin a potential instrument-reorder drag from the name strip.
        if (Fl::event_button() == FL_LEFT_MOUSE && timeline && overLabel) {
            int axisIdx = labelAxis;
            {
                const auto& lo = timeline->get().loopOrder;
                if (axisIdx < (int)lo.size()) {
                    dragAxisFrom  = axisIdx;
                    dragTrackId   = lo[axisIdx];
                    dragStartX    = mx;
                    dragStartY    = my;
                    draggingInstr = false;
                    dropGap       = -1;
                    take_focus();
                    return 1;
                }
            }
        }

        int trackIdx = -1, laneIdx = -1, col = -1, row = -1;
        if (!cellAt(mx, my, trackIdx, laneIdx, col, row)) return 0;
        take_focus();

        if (Fl::event_button() == FL_LEFT_MOUSE) {
            // Record a drag candidate. The toggle is deferred to FL_RELEASE so a
            // drag doesn't also toggle; a plain click (no drag) toggles there.
            if (timeline && trackIdx < (int)timeline->get().tracks.size()) {
                dragLaneId      = timeline->get().tracks[trackIdx].lanes[laneIdx].id;
                dragSrcTrackIdx = trackIdx;
                dragSrcLaneIdx  = laneIdx;
                dragCellStartX  = mx;
                dragCellStartY  = my;
                patternDragging = false;
                dropTrackIdx    = -1;
                dropSlot        = -1;
                dropForbidden   = false;
            }
            return 1;
        }
        if (Fl::event_button() == FL_RIGHT_MOUSE && contextPopup && patternObs) {
            const auto& tracks = patternObs->get().tracks;
            int trackId = trackIdx < (int)tracks.size() ? tracks[trackIdx].id : -1;
            int laneId  = -1;
            if (trackIdx < (int)tracks.size() && laneIdx < (int)tracks[trackIdx].lanes.size())
                laneId = tracks[trackIdx].lanes[laneIdx].id;
            contextPopup->open(trackId, laneId, patternObs,
                               Fl::event_x(), Fl::event_y());
            return 1;
        }
        return 0;
    }
    case FL_DRAG: {
        if (dragAxisFrom >= 0) {  // instrument-label drag
            if (!draggingInstr) {
                int d = tracksAsColumns ? std::abs(mx - dragStartX)
                                        : std::abs(my - dragStartY);
                if (d > dragThreshold) draggingInstr = true;
            }
            if (draggingInstr) {
                dropGap = computeDropGap(mx, my);
                // Two-headed cursor along the reorder axis: left/right when
                // instruments are columns (a row), up/down when they are rows.
                if (window())
                    window()->cursor(tracksAsColumns ? FL_CURSOR_WE : FL_CURSOR_NS);
                redraw();
            }
            return 1;
        }
        if (dragLaneId >= 0) {  // pattern-cell drag
            if (!patternDragging) {
                if (std::abs(mx - dragCellStartX) > dragThreshold ||
                    std::abs(my - dragCellStartY) > dragThreshold)
                    patternDragging = true;
            }
            if (patternDragging) {
                computePatternDrop(mx, my);
                if (window()) {
                    if (dropForbidden) window()->cursor(forbiddenCursorImage(), 0, 0);
                    else               window()->cursor(FL_CURSOR_MOVE);
                }
                redraw();
            }
            return 1;
        }
        return 0;
    }
    case FL_RELEASE: {
        if (dragAxisFrom >= 0) {  // instrument-label drag
            if (draggingInstr && timeline && dropGap >= 0) {
                const auto& lo = timeline->get().loopOrder;
                int insertBefore = (dropGap < (int)lo.size()) ? lo[dropGap] : -1;
                timeline->moveLoopInstrument(dragTrackId, insertBefore);
            }
            dragAxisFrom  = -1;
            draggingInstr = false;
            dropGap       = -1;
            redraw();
            return 1;
        }
        if (dragLaneId >= 0) {  // pattern-cell drag
            if (!patternDragging) {
                togglePattern(dragSrcTrackIdx, dragSrcLaneIdx);  // plain click
            } else if (timeline && !dropForbidden && dropTrackIdx >= 0) {
                const auto& tracks = timeline->get().tracks;
                int destTrackId  = tracks[dropTrackIdx].id;
                const auto& dl   = tracks[dropTrackIdx].loopLanes;
                int beforeLaneId = (dropSlot < (int)dl.size()) ? dl[dropSlot] : -1;
                timeline->moveLoopPattern(dragLaneId, destTrackId, beforeLaneId);
            }
            dragLaneId      = -1;
            patternDragging = false;
            dropTrackIdx    = -1;
            dropSlot        = -1;
            dropForbidden   = false;
            redraw();
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
    updateScrollbars();
}
