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

    // Growth rides the playhead's own timer rather than one of its own. It already
    // runs at 16-50 ms while the transport rolls, which is far finer than the one
    // check per bar growth actually needs, and the project has no polling threads to
    // add one to. Only the song editor sets onTick, so the pattern editors' copies
    // are free.
    playhead.onTick = [this]() { growTick(); };

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
    // Before the note is placed, so a pattern about to stop looping has already
    // gained the room the note may need.
    engageGrow();

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
    // Inside the still-open undo group, and after the commits above: the trim has to
    // see the notes this take just wrote, and the bars it removes belong to the same
    // ctrl-Z as the notes that caused them.
    endGrow();
    recUndo_.reset();   // the take is closed; the next one gets its own undo entry
}

// ── Flexible bars ("Grow") ───────────────────────────────────────────────────
//
// The engine never "wraps" a position at the end of a pattern. The drawn playhead
// (Playhead::patternBeat) and the RT sequencer (forEachFiring) both take the
// pattern's anchor and re-derive where they are modulo its length. So lengthening
// the pattern in time IS the feature: the modulo simply never comes round, the head
// runs on into the new bar and the sequencer stops re-firing the pattern's notes.
// Nothing new happens on the RT thread; it picks the change up in the snapshot it
// rebuilds for every other edit as well.
//
// The one precondition is that the pass in progress must be pass 0 — with the head
// in a later pass, changing the length moves the modulo boundary and the position
// jumps. See engageGrow() for how each mode is brought to that state.

// The Bars spinner tops out here (patternPanel.cpp), and growth must not produce a
// pattern that control cannot then express.
static constexpr int   kGrowMaxBars = 64;
// A hair of slack for float beats. quantiseBeat() is round(beat/q)*q, so a note that
// "ends exactly on the bar line" can land a few millionths past it; a bare ceil()
// would then buy it a whole empty bar.
static constexpr float kGrowEps     = 1.0e-3f;

float BasePatternEditor::patternBarBeats() const
{
    if (!pattern || lastPatId <= 0) return 0.0f;
    for (const auto& p : pattern->get().patterns)
        if (p.id == lastPatId) return (float)std::max(1, p.timeSigTop);
    return 0.0f;
}

float BasePatternEditor::patternLengthBeats() const
{
    if (!pattern || lastPatId <= 0) return 0.0f;
    for (const auto& p : pattern->get().patterns)
        if (p.id == lastPatId) return p.lengthBeats;
    return 0.0f;
}

const PatternInstance*
BasePatternEditor::growableInstance(float bars, const Playhead::PatternPos& pp) const
{
    if (!pattern || lastPatId <= 0) return nullptr;
    const auto& tl = pattern->get();
    if (lastSelectedTrack < 0 || lastSelectedTrack >= (int)tl.tracks.size()) return nullptr;
    const Track& track = tl.tracks[lastSelectedTrack];

    const Pattern* pat = nullptr;
    for (const auto& p : tl.patterns)
        if (p.id == lastPatId) { pat = &p; break; }
    if (!pat) return nullptr;

    // The lane the editor is showing, chosen exactly as patternIdForSelectedLane()
    // chooses it, so the block we grow is the one whose pattern is on screen.
    const Lane* lane = nullptr;
    for (const auto& l : track.lanes)
        if (l.id == lastSelectedLaneId) { lane = &l; break; }
    if (!lane && !track.lanes.empty()) lane = &track.lanes[0];
    if (!lane) return nullptr;

    for (const auto& inst : lane->patterns) {
        if (inst.patternId != lastPatId) continue;
        if (bars < inst.startBar || bars >= inst.startBar + inst.length) continue;
        // LoopManager::sync() reads the time signature at the block's start bar while
        // the playhead reads it at the current one. Across a time-signature marker
        // those disagree, and every conversion below would then be measured in one
        // unit and applied in the other — so leave such a take alone.
        const float instBpb = pattern->song()->patternBeatsPerBar((int)inst.startBar, *pat);
        if (instBpb <= 0.0f) return nullptr;
        if (std::fabs(instBpb - pp.beatsPerBar) > kGrowEps) return nullptr;
        // The placement has to be the one the phase came from; the same pattern
        // sitting on two lanes under the playhead would otherwise let us grow a block
        // that is not the one being heard.
        const float instAnchor = inst.startBar - inst.startOffset / instBpb;
        if (std::fabs(instAnchor - pp.anchorBar) > kGrowEps) return nullptr;
        return &inst;
    }
    return nullptr;
}

void BasePatternEditor::setGrowArmed(bool on)
{
    if (on == growArmed_) return;
    growArmed_ = on;
    // Switching it off stops the growth where it is and trims what was not played
    // into, rather than leaving a take growing that the user has said to stop.
    if (!on) endGrow();
}

void BasePatternEditor::engageGrow()
{
    if (!growArmed_ || growActive_ || !pattern || lastPatId <= 0) return;

    const float bars = playhead.transportBars();
    const Playhead::PatternPos pp = playhead.patternPos(bars);
    if (!pp.running || pp.beatsPerBar <= 0.0f) return;

    const float len = patternLengthBeats();
    const float bar = patternBarBeats();
    if (len <= 0.0f || bar <= 0.0f) return;

    const int cycle = (int)std::floor(pp.elapsedBeats / len);
    // Where the phase actually comes from. A Loop-Editor switch layered over Song
    // Mode counts as a loop: the Sequencer drops that pattern's song blocks and
    // plays it from the LoopManager anchor instead.
    const bool anchorIsOrigin = playhead.isLoopActive() || pp.manualLoop;

    if (anchorIsOrigin) {
        growInstId_         = 0;
        growBaseInstLength_ = 0.0f;
        // Advance the anchor by the whole passes already played. That is an exact
        // multiple of the pattern length, so it maps the firing set onto itself:
        // nothing moves and nothing is heard, but the pass in progress becomes pass
        // 0 and growing the pattern now extends it.
        if (cycle > 0)
            playhead.reanchorPattern(pp.anchorBar + (float)cycle * len / pp.beatsPerBar);
    } else {
        const PatternInstance* inst = growableInstance(bars, pp);
        if (!inst || cycle > 0) {
            // In Song Mode the engine takes its phase from the block, not from the
            // LoopManager, so there is nothing we can re-anchor: moving the anchor
            // alone would split the drawn head from what is played. Record normally.
            if (luvieDebug())
                fprintf(stderr, "[luvie] not growing: %s\n", !inst
                        ? "no usable pattern block under the playhead on the selected "
                          "lane - in Song mode the pattern grows with its block, so "
                          "there has to be exactly one, and no time signature change "
                          "inside it"
                        : "the block is longer than the pattern and the playhead is "
                          "past its first pass - start the take in the first pass");
            return;
        }
        growInstId_         = inst->id;
        growBaseInstLength_ = inst->length;
    }

    growBaseBeats_ = len;
    growActive_    = true;
    growTick();     // apply the invariant now, not one tick from now
}

void BasePatternEditor::growTick()
{
    if (!growActive_ || !pattern || lastPatId <= 0) return;
    if (!playhead.transportPlaying()) return;

    const Playhead::PatternPos pp = playhead.patternPos(playhead.transportBars());
    if (!pp.running || pp.beatsPerBar <= 0.0f) return;

    const float len = patternLengthBeats();
    const float bar = patternBarBeats();
    if (len <= 0.0f || bar <= 0.0f) { growActive_ = false; return; }

    // One whole empty bar beyond the bar the head is in. Growing exactly at the
    // boundary would put a tick interval - and in plugin mode a trip through the
    // host's worker thread - on the critical path; the spare bar takes both off it.
    const int head = (int)std::floor(pp.elapsedBeats / bar);
    float     want = (float)(head + 2) * bar;
    float     cap  = (float)kGrowMaxBars * bar;

    const PatternInstance* inst = nullptr;
    if (growInstId_) {
        inst = pattern->song()->instanceById(growInstId_);
        if (!inst) { growActive_ = false; growInstId_ = 0; return; }
        // Same-lane blocks may not overlap - SongGrid rejects a drag that would make
        // them - so stop where the next one starts. Recomputed every step, because
        // the neighbour can be dragged while the take runs.
        float nextStart = 0.0f;
        bool  haveNext  = false;
        const int laneId = pattern->song()->laneIdForInstance(growInstId_);
        for (const auto& track : pattern->get().tracks)
            for (const auto& lane : track.lanes) {
                if (lane.id != laneId) continue;
                for (const auto& other : lane.patterns) {
                    if (other.id == growInstId_) continue;
                    if (other.startBar <= inst->startBar) continue;
                    if (!haveNext || other.startBar < nextStart) {
                        nextStart = other.startBar;
                        haveNext  = true;
                    }
                }
            }
        if (haveNext)
            cap = std::min(cap, (nextStart - pp.anchorBar) * pp.beatsPerBar);
    }

    want = std::min(want, cap);
    // Capped is not finished: the take keeps recording (the pattern simply wraps
    // again, which is what the existing clip-at-the-end path already handles) and
    // growActive_ stays set so the end-of-take trim still runs.
    if (want <= len + kGrowEps) return;

    {
        ObservableSong::Batch batch(pattern->song());
        pattern->setPatternLength(lastPatId, want);
        if (growInstId_) {
            // Required, not cosmetic: the sequencer clamps a song instance's firing
            // window to its placement, sync() drops the pattern once the playhead
            // leaves it, and the song grid's length - which is what stops the
            // transport at the end of the song - is derived from it too.
            inst = pattern->song()->instanceById(growInstId_);   // the notify may move it
            if (!inst) { growActive_ = false; growInstId_ = 0; return; }
            const float need = pp.anchorBar + want / pp.beatsPerBar - inst->startBar;
            if (need > inst->length) pattern->song()->resizePattern(growInstId_, need);
        }
    }

    // Keep the head on screen as the grid outgrows the viewport.
    const int colW = gridColWidth();
    if (colW > 0) {
        const int visibleCols = gridWidgetW() / colW;
        const int headCol     = (int)pp.elapsedBeats;
        if (visibleCols > 1 && headCol >= colOffset + visibleCols - 1)
            setColOffset(headCol - visibleCols + 2);
    }
}

void BasePatternEditor::endGrow()
{
    if (!growActive_) return;
    growActive_ = false;
    const int   instId      = growInstId_;
    const float baseBeats   = growBaseBeats_;
    const float baseInstLen = growBaseInstLength_;
    growInstId_ = 0;

    if (!pattern || lastPatId <= 0) return;
    const Pattern* p = nullptr;
    for (const auto& q : pattern->get().patterns)
        if (q.id == lastPatId) { p = &q; break; }
    if (!p) return;                         // undone out from under us

    const float bar = (float)std::max(1, p->timeSigTop);

    // The bar after the last one anything sits in. Note the two predicates:
    // setPatternLength's truncation drops a note whose END is past the length but a
    // drum hit whose START is at or past it, so a hit exactly on the final bar line
    // needs the bar after it while a note ending exactly there does not.
    float used = 0.0f;
    for (const auto& n : p->notes)
        used = std::max(used, std::ceil((n.beat + n.length) / bar - kGrowEps) * bar);
    for (const auto& d : p->drumNotes)
        used = std::max(used, (std::floor(d.beat / bar) + 1.0f) * bar);
    // Automation survives a shrink either way (truncation leaves param lanes alone),
    // but a bar carrying a dot is not an empty bar.
    for (const auto& lane : p->paramLanes)
        for (const auto& pt : lane.points)
            if (!pt.anchor) used = std::max(used, (std::floor(pt.beat / bar) + 1.0f) * bar);

    // Never below what the pattern was before the take, and never below one bar.
    float finalBeats = std::max({ baseBeats, used, bar });
    if (finalBeats >= p->lengthBeats - kGrowEps) return;    // nothing grew, or nothing to give back

    ObservableSong::Batch batch(pattern->song());
    pattern->setPatternLength(lastPatId, finalBeats);
    if (instId) {
        const PatternInstance* inst = pattern->song()->instanceById(instId);
        if (!inst) return;
        const Pattern* pat = nullptr;
        for (const auto& q : pattern->get().patterns)
            if (q.id == lastPatId) { pat = &q; break; }
        if (!pat) return;
        const float bpb = pattern->song()->patternBeatsPerBar((int)inst->startBar, *pat);
        if (bpb <= 0.0f) return;
        const float anchor = inst->startBar - inst->startOffset / bpb;
        const float want   = std::max(baseInstLen,
                                      anchor + finalBeats / bpb - inst->startBar);
        if (want < inst->length) pattern->song()->resizePattern(instId, want);
    }
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

    // setColOffset() shows and hides the horizontal scrollbar, but the strip it
    // occupies is reserved by relayout(), so a length that crosses the overflow
    // threshold has to re-run the layout or the scrollbar appears over the bottom
    // row of notes. Flexible-bars recording crosses it mid-take; the Bars spinner
    // has always been able to cross it too.
    const bool hadHScroll = hScrollbar && hScrollbar->visible();
    setColOffset(colOffset);
    if (hScrollbar && hScrollbar->visible() != hadHScroll) {
        relayout();
        setColOffset(colOffset);
    }
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
