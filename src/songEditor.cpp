#include "songEditor.hpp"
#include "trackContextPopup.hpp"
#include "paramLaneContextPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <cstdio>

SongEditor::SongEditor(int x, int y, int visibleW,
                       int numRows, int numCols, int rowHeight, int colWidth,
                       float snap, Popup& popup)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      trackLabels(x, y + rulerH, labelW, numRows, rowHeight),
      trackControls(x + labelW, y + rulerH, controlsW, numRows, rowHeight),
      songGrid(numRows, numCols, rowHeight, colWidth, snap, popup),
      numVisibleRows(numRows)
{
    baseX        = x;
    rulerOffsetX = labelW + controlsW;  // no scrollbar initially

    const int gridH = numRows * rowHeight;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<SongEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setRowOffset((int)sb->value());
    }, this);
    scrollbar->hide();

    hScrollbar = new GridScrollPane(x + labelW + controlsW, y + rulerH + numRows * rowHeight,
                                    visibleW - labelW - controlsW, hScrollH,
                                    GridScrollPane::HORIZONTAL);
    hScrollbar->linesize(1);
    hScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<SongEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setColOffset((int)sb->value());
    }, this);
    hScrollbar->hide();

    trackLabels.position(x, y + rulerH);
    trackControls.position(x + labelW, y + rulerH);
    songGrid.position(x + labelW + controlsW, y + rulerH);
    songGrid.setPlayhead(&playhead);

    add(*scrollbar);
    add(*hScrollbar);
    add(trackLabels);
    add(trackControls);
    add(songGrid);

    playhead.setOwner(this);
    end();
}

void SongEditor::drawRulerLabels()
{
    if (songGrid.colWidth <= 0) return;

    fl_font(FL_HELVETICA, 9);
    fl_color(0xA0906000);   // dim warm tone on yellow ruler bg

    int gridLeft     = x() + rulerOffsetX;
    int gridRight    = x() + w();
    int pixelBase    = x() + rulerOffsetX - hScrollPixel;
    int textBaseline = y() + rulerH / 2 + (fl_height() - fl_descent()) / 2;

    int firstCol = songGrid.colWidth > 0 ? hScrollPixel / songGrid.colWidth : 0;
    for (int col = firstCol; col < songGrid.numCols; ++col) {
        int cx = pixelBase + col * songGrid.colWidth + songGrid.colWidth / 2;
        if (cx < gridLeft)  continue;
        if (cx >= gridRight) break;

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", col + 1);
        int tw = (int)fl_width(buf);
        fl_draw(buf, cx - tw / 2, textBaseline);
    }
}

SongEditor::~SongEditor()
{
    swapObserver(timeline, nullptr, this);
}

void SongEditor::setTransport(ITransport* t, ObservableSong* tl)
{
    transport = t;
    swapObserver(timeline, tl, this);
    playhead.setTransport(t, tl);
    playhead.onEndReached = [this]() { if (onEndReached) onEndReached(); };
    playhead.onTick       = [this]() { followPlayhead(); };
    songGrid.onPatternDoubleClick = [this](int i, int laneId) { if (onPatternDoubleClick) onPatternDoubleClick(i, laneId); };
    songGrid.onOpenPattern        = [this](int i, int laneId) { if (onOpenPattern) onOpenPattern(i, laneId); };
    songGrid.setTimeline(tl);
    trackLabels.setTimeline(tl);
    trackControls.setTimeline(tl);
    updateScrollBounds();
}

void SongEditor::setPattern(ObservablePattern* p)
{
    trackLabels.setPattern(p);
}

void SongEditor::setContextPopup(TrackContextPopup* p)
{
    trackLabels.setContextPopup(p);
}

void SongEditor::setParamLaneContextPopup(ParamLaneContextPopup* p)
{
    trackLabels.setParamLaneContextPopup(p);
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

int SongEditor::computeNumCols() const
{
    static constexpr int minCols = 60;
    static constexpr int step    = 20;
    if (!timeline) return minCols;
    float maxEnd = 0.0f;
    const auto& data = timeline->get();
    for (const auto& track : data.tracks)
        for (const auto& lane : track.lanes)
            for (const auto& inst : lane.patterns)
                maxEnd = std::max(maxEnd, inst.startBar + inst.length);
    for (const auto& m : data.bpms)
        maxEnd = std::max(maxEnd, (float)m.bar);
    for (const auto& m : data.timeSigs)
        maxEnd = std::max(maxEnd, (float)m.bar);
    int rounded = (int)(std::ceil(maxEnd / step) * step);
    return std::max(minCols, rounded + step);
}

void SongEditor::updateScrollBounds()
{
    int newNumCols = computeNumCols();
    if (newNumCols != songGrid.numCols) {
        songGrid.numCols = newNumCols;
        if (onNumColsChanged) onNumColsChanged(newNumCols);
    }
    if (!timeline || !scrollbar) return;
    int total = (int)timeline->get().rowOrder.size();
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

    // Reposition labels, controls, and grid based on scrollbar presence
    trackLabels.position(baseX + sbW, trackLabels.y());
    trackControls.position(baseX + sbW + labelW, trackControls.y());
    songGrid.position(baseX + sbW + labelW + controlsW, songGrid.y());
    rulerOffsetX = sbW + labelW + controlsW;

    // Horizontal scroll
    int visibleGridW  = std::max(0, w() - sbW - labelW - controlsW);
    int totalGridW    = songGrid.numCols * songGrid.colWidth;
    bool needsHScroll = totalGridW > visibleGridW;
    int visibleCols   = songGrid.colWidth > 0 ? visibleGridW / songGrid.colWidth : 0;

    if (needsHScroll) {
        colOffset = std::clamp(colOffset, 0, std::max(0, songGrid.numCols - visibleCols));
        hScrollbar->value(colOffset, visibleCols, 0, songGrid.numCols);
        hScrollbar->show();
    } else {
        colOffset = 0;
        hScrollbar->hide();
    }
    hScrollbar->position(baseX + sbW + labelW + controlsW, y() + rulerH + gridH);
    hScrollbar->size(visibleGridW, hScrollH);

    hScrollPixel = colOffset * songGrid.colWidth;
    songGrid.setColOffset(colOffset);

    if (onRulerOffsetChanged) onRulerOffsetChanged(rulerOffsetX - hScrollPixel, rulerOffsetX);

    // Resize children to match actual track count (up to numVisibleRows)
    songGrid.numRows = visRows;
    songGrid.size(visibleGridW, gridH);
    trackLabels.setNumVisibleRows(visRows);
    trackLabels.size(trackLabels.w(), gridH);
    trackControls.setNumVisibleRows(visRows);
    trackControls.size(trackControls.w(), gridH);

    songGrid.setRowOffset(rowOffset);
    trackLabels.setRowOffset(rowOffset);
    trackControls.setRowOffset(rowOffset);
    redraw();
}

void SongEditor::setRowOffset(int offset)
{
    if (!timeline || !scrollbar) return;
    int total = (int)timeline->get().rowOrder.size();
    int maxOff = std::max(0, total - numVisibleRows);
    rowOffset  = std::clamp(offset, 0, maxOff);
    if (total > numVisibleRows) {
        int shown = std::max(total, numVisibleRows);
        scrollbar->value(rowOffset, numVisibleRows, 0, shown);
    }
    songGrid.setRowOffset(rowOffset);
    trackLabels.setRowOffset(rowOffset);
    trackControls.setRowOffset(rowOffset);
}

void SongEditor::setColOffset(int offset)
{
    if (!hScrollbar) return;
    int visibleGridW = std::max(0, w() - (scrollbar && scrollbar->visible() ? scrollbarW : 0) - labelW - controlsW);
    int visibleCols  = songGrid.colWidth > 0 ? visibleGridW / songGrid.colWidth : 0;
    colOffset  = std::clamp(offset, 0, std::max(0, songGrid.numCols - visibleCols));
    hScrollPixel = colOffset * songGrid.colWidth;
    songGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, songGrid.numCols);
    if (onRulerOffsetChanged) onRulerOffsetChanged(rulerOffsetX - hScrollPixel, rulerOffsetX);
    redraw();
}

void SongEditor::followPlayhead()
{
    if (!transport) return;
    bool  playing = transport->isPlaying();
    float bar     = transport->position();

    int   sbW          = (scrollbar && scrollbar->visible()) ? scrollbarW : 0;
    int   visibleGridW = std::max(1, w() - sbW - labelW - controlsW);
    int   visibleCols  = songGrid.colWidth > 0 ? visibleGridW / songGrid.colWidth : 1;
    float playheadPx   = (bar - colOffset) * songGrid.colWidth;

    if (playing) {
        if (!wasPlaying) {
            // Just started — snap if playhead is off screen
            if (playheadPx < 0.0f || playheadPx >= (float)visibleGridW)
                setColOffset((int)bar);
        } else {
            // Page forward the moment the playhead crosses the visible right edge
            if (playheadPx >= (float)visibleGridW)
                setColOffset(colOffset + visibleCols);
        }
    }
    wasPlaying = playing;
}

void SongEditor::resize(int x, int /*y*/, int w, int h)
{
    Fl_Widget::resize(x, y(), w, h);
    baseX          = x;
    numVisibleRows = std::max(1, (h - rulerH - hScrollH) / songGrid.rowHeight);
    updateScrollBounds();
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
