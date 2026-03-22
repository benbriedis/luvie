#include "songEditor.hpp"
#include "trackContextPopup.hpp"
#include <FL/Fl.H>
#include <algorithm>

SongEditor::SongEditor(int x, int y, int visibleW, std::vector<Note> notes,
                       int numRows, int numCols, int rowHeight, int colWidth,
                       float snap, Popup& popup)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      trackLabels(x, y + rulerH, labelW, numRows, rowHeight),
      songGrid(notes, numRows, numCols, rowHeight, colWidth, snap, popup),
      numVisibleRows(numRows)
{
    baseX        = x;
    rulerOffsetX = labelW;  // no scrollbar initially

    const int gridH = numRows * rowHeight;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<SongEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setRowOffset((int)sb->value());
    }, this);
    scrollbar->hide();

    hScrollbar = new GridScrollPane(x + labelW, y + rulerH + numRows * rowHeight,
                                    visibleW - labelW, hScrollH,
                                    GridScrollPane::HORIZONTAL);
    hScrollbar->linesize(1);
    hScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<SongEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setColOffset((int)sb->value());
    }, this);
    hScrollbar->hide();

    trackLabels.position(x, y + rulerH);
    songGrid.position(x + labelW, y + rulerH);
    songGrid.setPlayhead(&playhead);

    add(*scrollbar);
    add(*hScrollbar);
    add(trackLabels);
    add(songGrid);

    playhead.setOwner(this);
    end();
}

SongEditor::~SongEditor()
{
    swapObserver(timeline, nullptr, this);
}

void SongEditor::setTransport(ITransport* t, ObservableTimeline* tl)
{
    swapObserver(timeline, tl, this);
    playhead.setTransport(t, tl);
    playhead.onEndReached = [this]() { if (onEndReached) onEndReached(); };
    songGrid.onPatternDoubleClick = [this](int i) { if (onPatternDoubleClick) onPatternDoubleClick(i); };
    songGrid.setTimeline(tl);
    trackLabels.setTimeline(tl);
    updateScrollBounds();
}

void SongEditor::setContextPopup(TrackContextPopup* p)
{
    trackLabels.setContextPopup(p);
}

void SongEditor::setTrackView(int trackIndex, bool beatResolution)
{
    songGrid.setTrackView(trackIndex, beatResolution);
    playhead.setPatternTrack(trackIndex);
}

void SongEditor::onTimelineChanged()
{
    updateScrollBounds();
}

void SongEditor::updateScrollBounds()
{
    if (!timeline || !scrollbar) return;
    int total    = (int)timeline->get().tracks.size();
    int visRows  = std::min(total, numVisibleRows);
    int rh       = songGrid.rowHeight;
    int gridH    = std::max(visRows * rh, 1);
    int maxOff   = std::max(0, total - numVisibleRows);
    rowOffset    = std::clamp(rowOffset, 0, maxOff);

    bool needsScroll = (total > numVisibleRows);
    int  sbW         = needsScroll ? scrollbarW : 0;

    // Reposition / resize scrollbar
    scrollbar->position(baseX, scrollbar->y());
    scrollbar->size(scrollbarW, gridH);
    if (needsScroll) {
        int shown = std::max(total, numVisibleRows);
        scrollbar->value(rowOffset, numVisibleRows, 0, shown);
        scrollbar->show();
    } else {
        scrollbar->hide();
    }

    // Reposition labels and grid based on scrollbar presence
    trackLabels.position(baseX + sbW, trackLabels.y());
    songGrid.position(baseX + sbW + labelW, songGrid.y());
    rulerOffsetX = sbW + labelW;

    // Horizontal scroll
    int visibleGridW = w() - sbW - labelW;
    int totalGridW   = songGrid.numCols * songGrid.colWidth;
    bool needsHScroll = totalGridW > visibleGridW;
    int visibleCols   = visibleGridW / songGrid.colWidth;

    if (needsHScroll) {
        colOffset = std::clamp(colOffset, 0, songGrid.numCols - visibleCols);
        hScrollbar->value(colOffset, visibleCols, 0, songGrid.numCols);
        hScrollbar->show();
    } else {
        colOffset = 0;
        hScrollbar->hide();
    }
    hScrollbar->position(baseX + sbW + labelW, y() + rulerH + gridH);
    hScrollbar->size(visibleGridW, hScrollH);

    hScrollPixel = colOffset * songGrid.colWidth;
    songGrid.setColOffset(colOffset);

    if (onRulerOffsetChanged) onRulerOffsetChanged(rulerOffsetX - hScrollPixel, rulerOffsetX);

    // Resize children to match actual track count (up to numVisibleRows)
    songGrid.numRows = visRows;
    songGrid.size(visibleGridW, gridH);
    trackLabels.setNumVisibleRows(visRows);
    trackLabels.size(trackLabels.w(), gridH);

    songGrid.setRowOffset(rowOffset);
    trackLabels.setRowOffset(rowOffset);
    redraw();
}

void SongEditor::setRowOffset(int offset)
{
    if (!timeline || !scrollbar) return;
    int total  = (int)timeline->get().tracks.size();
    int maxOff = std::max(0, total - numVisibleRows);
    rowOffset  = std::clamp(offset, 0, maxOff);
    if (total > numVisibleRows) {
        int shown = std::max(total, numVisibleRows);
        scrollbar->value(rowOffset, numVisibleRows, 0, shown);
    }
    songGrid.setRowOffset(rowOffset);
    trackLabels.setRowOffset(rowOffset);
}

void SongEditor::setColOffset(int offset)
{
    if (!hScrollbar) return;
    int visibleGridW = w() - (scrollbar && scrollbar->visible() ? scrollbarW : 0) - labelW;
    int visibleCols  = visibleGridW / songGrid.colWidth;
    colOffset  = std::clamp(offset, 0, std::max(0, songGrid.numCols - visibleCols));
    hScrollPixel = colOffset * songGrid.colWidth;
    songGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, songGrid.numCols);
    if (onRulerOffsetChanged) onRulerOffsetChanged(rulerOffsetX - hScrollPixel, rulerOffsetX);
    redraw();
}

int SongEditor::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        if (Fl::event_dx() != 0)
            setColOffset(colOffset + Fl::event_dx());
        else
            setRowOffset(rowOffset + Fl::event_dy());
        return 1;
    }
    return Editor::handle(event);
}
