#include "basePatternEditor.hpp"
#include "noteAuditioner.hpp"
#include <FL/Fl.H>
#include <algorithm>

BasePatternEditor::BasePatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                                     int rowHeight, int colWidth, float snap, int lw)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      paramLabels(x + scrollbarW, y + rulerH + numRows * rowHeight, lw),
      paramGrid(x + scrollbarW + lw, y + rulerH + numRows * rowHeight,
                visibleW - scrollbarW - lw, colWidth, snap)
{
    rulerOffsetX = scrollbarW + lw;
    seekingEnabled = false;

    const int gridH        = numRows * rowHeight;
    const int paramY       = y + rulerH + gridH;
    const int visibleGridW = visibleW - scrollbarW - lw;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<BasePatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        int maxOff = std::max(0, self->totalRows() - self->gridNumRows());
        self->setRowOffset(maxOff - (int)sb->value());
    }, this);

    paramScrollbar = new GridScrollPane(x, paramY, scrollbarW, kParamAreaH);
    paramScrollbar->linesize(1);
    paramScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<BasePatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->paramLaneOffset = (int)sb->value();
        self->paramGrid.setLaneOffset(self->paramLaneOffset);
        self->paramLabels.setLaneOffset(self->paramLaneOffset);
    }, this);
    paramScrollbar->hide();

    hScrollbar = new GridScrollPane(x + scrollbarW + lw, paramY,
                                    visibleGridW, hScrollH, GridScrollPane::HORIZONTAL);
    hScrollbar->linesize(1);
    hScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<BasePatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setColOffset((int)sb->value());
    }, this);
    hScrollbar->hide();

    if (numCols * colWidth > visibleGridW) {
        hScrollbar->value(0, visibleGridW / colWidth, 0, numCols);
        hScrollbar->show();
    }

    paramLabels.hide();
    paramGrid.hide();
    paramGrid.setNumCols(numCols);

    // Establish child ordering; subclass ctors append their own widgets after these
    add(*scrollbar);
    add(*paramScrollbar);
    add(*hScrollbar);
}

BasePatternEditor::~BasePatternEditor()
{
    swapObserver(pattern, nullptr, this);
}

void BasePatternEditor::setPatternPlayhead(ITransport* t, ObservablePattern* pat, int trackIndex)
{
    swapObserver(pattern, pat, this);
    playhead.setTransport(t, pat ? pat->song() : nullptr);
    playhead.setPatternTrack(trackIndex);
}

int BasePatternEditor::currentInstrumentId() const
{
    if (!pattern) return 0;
    const auto& tl  = pattern->get();
    int         sel = tl.selectedTrackIndex;
    if (sel < 0 || sel >= (int)tl.tracks.size()) return 0;
    int patId = tl.tracks[sel].lanes.empty() ? 0 : tl.tracks[sel].lanes[0].patternId;
    for (const auto& p : tl.patterns)
        if (p.id == patId) return p.instrumentId;
    return 0;
}

void BasePatternEditor::setAuditioner(NoteAuditioner* a)
{
    auditioner = a;
    labelsSetOnRowClicked([this](int midi) {
        if (auditioner && midi >= 0)
            auditioner->play(currentInstrumentId(), midi, 100, 0.5f);
    });
}

void BasePatternEditor::setNoteLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    labelsSetOnRightClick([this, popup]() {
        if (!popup || !pattern || lastSelectedTrack < 0) return;
        if (lastSelectedTrack >= (int)pattern->get().tracks.size()) return;
        int patId = pattern->get().patternIdForSelectedLane();
        popup->open(
            Fl::event_x(), Fl::event_y(),
            [this, patId](const char* type) { return pattern->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); }
        );
    });
}

void BasePatternEditor::setParamLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    paramLabels.onRightClick = [this, popup](int laneId) {
        if (!popup || !pattern || lastSelectedTrack < 0) return;
        if (lastSelectedTrack >= (int)pattern->get().tracks.size()) return;
        int patId = pattern->get().patternIdForSelectedLane();
        std::function<void()> onRemove;
        if (laneId >= 0)
            onRemove = [this, laneId]() { pattern->removePatternParamLane(laneId); };
        popup->open(
            Fl::event_x(), Fl::event_y(),
            [this, patId](const char* type) { return pattern->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); },
            std::move(onRemove)
        );
    };
}

void BasePatternEditor::onTimelineChanged()
{
    if (!pattern) return;
    const auto& tl       = pattern->get();
    int         sel      = tl.selectedTrackIndex;
    int         selLane  = tl.selectedLaneId;
    bool trackChanged = (sel != lastSelectedTrack) || (selLane != lastSelectedLaneId);
    lastSelectedTrack  = sel;
    lastSelectedLaneId = selLane;

    if (sel < 0 || sel >= (int)tl.tracks.size()) {
        afterTimelineChanged(-1);
        return;
    }
    int patId = tl.patternIdForSelectedLane();
    bool patChanged = (patId != lastPatId);
    lastPatId = patId;

    if (trackChanged) playhead.setPatternTrack(sel);
    if (trackChanged || patChanged) {
        setGridPattern(patId);
        paramGrid.setPattern(pattern, patId);
    } else {
        paramGrid.update(pattern, patId);
    }
    paramLabels.setPattern(pattern, patId);
    updateParamScrollbar();
    afterTimelineChanged(patId);
}

void BasePatternEditor::setRowOffset(int offset)
{
    int total  = totalRows();
    int maxOff = std::max(0, total - gridNumRows());
    offset = std::clamp(offset, 0, maxOff);
    labelsSetRowOffset(offset);
    gridSetRowOffset(offset);
    if (scrollbar)
        scrollbar->value(maxOff - offset, gridNumRows(), 0, total);
}

void BasePatternEditor::setColOffset(int offset)
{
    if (!hScrollbar) return;
    int visibleCols = gridWidgetW() / gridColWidth();
    colOffset = std::clamp(offset, 0, std::max(0, gridNumCols() - visibleCols));
    hScrollPixel = colOffset * gridColWidth();
    gridSetColOffset(colOffset);
    paramGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, gridNumCols());

    if (gridNumCols() * gridColWidth() > gridWidgetW())
        hScrollbar->show();
    else
        hScrollbar->hide();
    redraw();
}

void BasePatternEditor::updateParamScrollbar()
{
    int total = paramGrid.numLanes();
    if (total > kMaxVisParams) {
        int maxOff = total - kMaxVisParams;
        paramLaneOffset = std::clamp(paramLaneOffset, 0, maxOff);
        if (paramScrollbar)
            paramScrollbar->value(paramLaneOffset, kMaxVisParams, 0, total);
    } else {
        paramLaneOffset = 0;
    }
    paramGrid.setLaneOffset(paramLaneOffset);
    paramLabels.setLaneOffset(paramLaneOffset);
    relayout();
}

void BasePatternEditor::relayout()
{
    const int bx = x(), gy = y(), bw = w(), bh = h();
    const int lanes        = paramGrid.numLanes();
    const int visRows      = std::min(lanes, kMaxVisParams);
    const int paramAreaH   = visRows * kParamRowH;
    const int lw           = labelsWidth();
    const int visibleGridW = std::max(1, bw - scrollbarW - lw);

    // Only reserve a row for the horizontal scrollbar when it is actually shown.
    const bool hbarShown = gridNumCols() * gridColWidth() > visibleGridW;
    const int  hbarH     = hbarShown ? hScrollH : 0;

    // The note scrollbar, labels and grid share one height that fills the area
    // down to the param lanes / horizontal scrollbar / control bar. Only whole
    // rows are interactive; the sub-row remainder shows as background below the
    // last row rather than leaving a gap above the control bar.
    const int noteAreaH  = std::max(gridRowHeight(), bh - rulerH - paramAreaH - hbarH);
    const int newNumRows = std::max(1, noteAreaH / gridRowHeight());
    const int noteTop    = gy + rulerH;
    const int paramY     = noteTop + noteAreaH;

    scrollbar->resize(bx, noteTop, scrollbarW, noteAreaH);
    labelsSetNumRows(newNumRows);
    labelsResize(bx + scrollbarW, noteTop, lw, noteAreaH);
    gridSetNumRows(newNumRows);
    gridResize(bx + scrollbarW + lw, noteTop, visibleGridW, noteAreaH);

    if (paramAreaH > 0) {
        paramLabels.resize(bx + scrollbarW, paramY, lw, paramAreaH);
        paramGrid.resize(bx + scrollbarW + lw, paramY, visibleGridW, paramAreaH);
        paramLabels.show();
        paramGrid.show();
        if (lanes > kMaxVisParams) {
            paramScrollbar->resize(bx, paramY, scrollbarW, paramAreaH);
            paramScrollbar->show();
        } else {
            paramScrollbar->hide();
        }
    } else {
        paramLabels.hide();
        paramGrid.hide();
        paramScrollbar->hide();
    }

    hScrollbar->resize(bx + scrollbarW + lw, paramY + paramAreaH, visibleGridW, hScrollH);

    setRowOffset(currentRowOffset());
    setColOffset(colOffset);
}

void BasePatternEditor::resize(int x, int /*y*/, int w, int h)
{
    Fl_Widget::resize(x, y(), w, h);
    relayout();
}

int BasePatternEditor::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        if (Fl::event_dx() != 0)
            setColOffset(colOffset + Fl::event_dx());
        else
            setRowOffset(currentRowOffset() - Fl::event_dy());
        return 1;
    }
    return Editor::handle(event);
}
