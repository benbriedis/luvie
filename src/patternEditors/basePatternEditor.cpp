// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "basePatternEditor.hpp"
#include "noteAuditioner.hpp"
#include "luvieDebug.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <cmath>

BasePatternEditor::BasePatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                                     int rowHeight, int colWidth, float snap, int lw)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      gridPane(x, y + rulerH, visibleW, numRows * rowHeight + hScrollH),
      paramLabels(x + scrollbarW, y + rulerH + numRows * rowHeight, lw),
      paramGrid(x + scrollbarW + lw, y + rulerH + numRows * rowHeight,
                visibleW - scrollbarW - lw, colWidth, snap)
{
    rulerOffsetX = scrollbarW + lw;
    seekingEnabled = false;
    baseColWidth   = colWidth;
    snapBeats_     = snap;

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

    // The scrolling body lives in gridPane (sized to content in relayout()).
    // Establish child ordering; subclass ctors append their own widgets after these.
    gridPane.add(*scrollbar);
    gridPane.add(*paramScrollbar);
    gridPane.add(*hScrollbar);
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
    const auto& tl    = pattern->get();
    int         patId = tl.patternIdForSelectedLane();
    for (const auto& p : tl.patterns)
        if (p.id == patId) return p.instrumentId;
    return 0;
}

// ── MIDI input and recording ─────────────────────────────────────────────────

void BasePatternEditor::setRecordArmed(bool on)
{
    if (on == recArmed_) return;
    recArmed_ = on;
    // Disarming ends the take: held notes are committed with the length they
    // reached, and the undo group closes so the next take is its own entry.
    if (!on) releaseMidiNotes();
}

void BasePatternEditor::midiNoteOn(int pitch, int velocity)
{
    if (pitch < 0 || pitch > 127) return;
    if (velocity <= 0) { midiNoteOff(pitch); return; }   // running-status note-off

    // Always audible, armed or not: trying a note out is half of what a keyboard
    // is for, and it costs nothing when the transport is stopped.
    if (auditioner) auditioner->noteOn(currentInstrumentId(), pitch, velocity);

    if (!recArmed_ || !canRecord() || !pattern) return;

    // Three things have to be true to record, and "nothing happened" looks the
    // same for all of them, so say which one failed when tracing is on.
    const bool rolling = playhead.transportPlaying();
    const float beat   = rolling ? playhead.patternBeat(playhead.transportBars()) : -1.0f;
    if (luvieDebug() && (!rolling || beat < 0.0f))
        fprintf(stderr, "[luvie] not recorded: %s\n", !rolling
                ? "transport is not rolling"
                : "this pattern is not playing right now - in Song mode the "
                  "playhead has to be inside one of its blocks, in Loop mode it "
                  "has to be switched on in the Loop Editor");
    if (!rolling || beat < 0.0f) return;

    if (!recUndo_) recUndo_.emplace(pattern->song());    // the take starts here

    if (recordsOnNoteOn()) {
        // No length to work out, but the start is quantised like any other.
        float start = 0.0f, len = 0.0f;
        recordedNoteSpan(beat, beat, start, len);
        commitRecordedNote(pitch, start, len, velocity);
        return;
    }
    recNotes_.push_back({pitch, beat, velocity});
}

float BasePatternEditor::quantiseBeat(float beat) const
{
    if (snapBeats_ <= 0.0f) return beat;            // Snap off: play it as played
    return std::round(beat / snapBeats_) * snapBeats_;
}

void BasePatternEditor::recordedNoteSpan(float rawStart, float rawEnd,
                                         float& startBeat, float& lenBeats) const
{
    const float total = (float)recordPatternBeats();
    const float q     = snapBeats_;

    startBeat = quantiseBeat(rawStart);
    // Rounding up can put the start on the end of the pattern, where no note
    // fits; the last division is the nearest place it can actually go.
    if (q > 0.0f) startBeat = std::clamp(startBeat, 0.0f, std::max(0.0f, total - q));

    const float room = total - startBeat;   // what is left of the pattern
    if (rawEnd < rawStart) {                // the pattern wrapped under the key
        lenBeats = room;
        return;
    }

    lenBeats = quantiseBeat(rawEnd) - startBeat;
    // Start and end rounded to the same division: a note was played, so it gets
    // the shortest one the grid can show rather than vanishing.
    if (q > 0.0f && lenBeats < q) lenBeats = q;
    if (lenBeats > room) lenBeats = room;
}

void BasePatternEditor::midiNoteOff(int pitch)
{
    if (auditioner) auditioner->noteOff(currentInstrumentId(), pitch);

    for (int i = (int)recNotes_.size() - 1; i >= 0; i--) {
        if (recNotes_[i].pitch != pitch) continue;
        const RecordingNote n = recNotes_[i];
        recNotes_.erase(recNotes_.begin() + i);

        const float end = playhead.patternBeat(playhead.transportBars());
        if (end >= 0.0f && pattern) {
            float start = 0.0f, len = 0.0f;
            recordedNoteSpan(n.startBeat, end, start, len);
            commitRecordedNote(n.pitch, start, len, n.velocity);
        }
        break;   // one note-off releases one note-on
    }
}

void BasePatternEditor::releaseMidiNotes()
{
    // Commit before silencing: patternBeat() is still meaningful here, and a key
    // the user is holding when they disarm is a note they played.
    if (!recNotes_.empty()) {
        auto pending = recNotes_;
        recNotes_.clear();
        for (const auto& n : pending) {
            if (auditioner) auditioner->noteOff(currentInstrumentId(), n.pitch);
            if (!pattern) continue;
            const float end = playhead.patternBeat(playhead.transportBars());
            if (end < 0.0f) continue;
            float start = 0.0f, len = 0.0f;
            recordedNoteSpan(n.startBeat, end, start, len);
            commitRecordedNote(n.pitch, start, len, n.velocity);
        }
    }
    recUndo_.reset();   // the take is closed; the next one gets its own undo entry
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
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); },
            {},                       // no "Remove automation" from the note labels
            labelsRenameHandler()
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

    // A take belongs to the pattern it was played into. Close it before lastPatId
    // moves, or a key still held while the user selects another pattern would have
    // its note committed to that one instead.
    if ((patChanged || trackChanged) && (!recNotes_.empty() || recUndo_))
        releaseMidiNotes();

    lastPatId = patId;

    if (trackChanged) playhead.setPatternTrack(sel);
    if (trackChanged || patChanged) {
        setGridPattern(patId);
        paramGrid.setPattern(pattern, patId);
        lastLengthBeats = -1.0f;
    } else {
        paramGrid.update(pattern, patId);
    }
    applyPatternLength(patId);
    paramLabels.setPattern(pattern, patId);
    updateParamScrollbar();
    afterTimelineChanged(patId);
}

// Keep the visible column count in step with the pattern's length (the Bars
// control edits lengthBeats; every editor renders one column per beat).
void BasePatternEditor::applyPatternLength(int patId)
{
    if (patId <= 0 || !pattern) return;
    float lb = 0.0f;
    for (const auto& p : pattern->get().patterns)
        if (p.id == patId) { lb = p.lengthBeats; break; }

    if (lb <= 0.0f || lb == lastLengthBeats) return;
    lastLengthBeats = lb;

    gridSetNumCols((int)lb);
    paramGrid.setNumCols((int)lb);
    playhead.setNumCols((int)lb);
    setColOffset(colOffset);
    redraw();
}

void BasePatternEditor::setZoom(int factor)
{
    int cw = baseColWidth * std::max(1, factor);
    if (cw <= 0 || cw == gridColWidth()) return;
    gridSetColWidth(cw);
    paramGrid.setColWidth(cw);
    playhead.setColWidth(cw);
    relayout();
    redraw();
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
    const int rowH         = gridRowHeight();

    // Reserve the horizontal-scrollbar strip only when it is actually shown
    // (i.e. the columns overflow the visible width). When the pattern fits, the
    // note area extends all the way down to the control bar instead of leaving a
    // permanent empty strip. Only whole rows are interactive.
    const int hScrollReserve = (gridNumCols() * gridColWidth() > visibleGridW) ? hScrollH : 0;
    const int  availNoteH   = std::max(rowH, bh - rulerH - paramAreaH - hScrollReserve);
    const int  contentNoteH = totalRows() * rowH;
    const bool needsVScroll = contentNoteH > availNoteH;
    const int  noteAreaH    = std::max(rowH, needsVScroll ? (availNoteH / rowH) * rowH
                                                          : contentNoteH);
    const int newNumRows = std::max(1, noteAreaH / rowH);
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

    // The scrolling body pane is sized to the content (plus the horizontal-scrollbar
    // strip when one is shown); any gap below it down to the control bar is painted
    // as clean background by Editor::draw().
    gridPane.resize(bx, noteTop, bw, noteAreaH + paramAreaH + hScrollReserve);

    setRowOffset(currentRowOffset());
    setColOffset(colOffset);
}
