#include "basePatternEditor.hpp"
#include <FL/Fl.H>
#include <algorithm>

BasePatternEditor::BasePatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                                     int rowHeight, int colWidth, float snap, int lw)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      paramLabels(x + scrollbarW, y + rulerH + numRows * rowHeight, lw),
      paramGrid(x + scrollbarW + lw, y + rulerH + numRows * rowHeight,
                visibleW - scrollbarW - lw, colWidth, snap)
{}

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

void BasePatternEditor::setNoteLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    labelsSetOnRightClick([this, popup]() {
        if (!popup || !pattern || lastSelectedTrack < 0) return;
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack >= (int)tracks.size()) return;
        int patId = tracks[lastSelectedTrack].patternId;
        popup->open(
            Fl::event_x_root(), Fl::event_y_root(),
            [this, patId](const char* type) { return pattern->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); }
        );
    });
}

void BasePatternEditor::setParamLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    paramLabels.onRightClick = [this, popup](int laneId) {
        if (!popup || !pattern || lastSelectedTrack < 0) return;
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack >= (int)tracks.size()) return;
        int patId = tracks[lastSelectedTrack].patternId;
        std::function<void()> onRemove;
        if (laneId >= 0)
            onRemove = [this, laneId]() { pattern->removePatternParamLane(laneId); };
        popup->open(
            Fl::event_x_root(), Fl::event_y_root(),
            [this, patId](const char* type) { return pattern->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); },
            std::move(onRemove)
        );
    };
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
    const int lanes      = paramGrid.numLanes();
    const int visRows    = std::min(lanes, kMaxVisParams);
    const int paramAreaH = visRows * kParamRowH;
    const int lw         = labelsWidth();

    const int newNumRows   = std::max(1, (bh - rulerH - paramAreaH - hScrollH) / gridRowHeight());
    const int visibleGridW = std::max(1, bw - scrollbarW - lw);
    const int gridH        = newNumRows * gridRowHeight();
    const int paramY       = gy + rulerH + gridH;

    scrollbar->resize(bx, gy + rulerH, scrollbarW, gridH);
    labelsSetNumRows(newNumRows);
    labelsResize(bx + scrollbarW, gy + rulerH, lw, gridH);
    gridSetNumRows(newNumRows);
    gridResize(bx + scrollbarW + lw, gy + rulerH, visibleGridW, gridH);

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
