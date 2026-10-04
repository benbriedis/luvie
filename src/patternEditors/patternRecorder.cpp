// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "patternRecorder.hpp"
#include "loopManager.hpp"
#include "luvieDebug.hpp"
#include "paramLaneTypes.hpp"
#include <algorithm>
#include <cmath>

PatternRecorder::PatternRecorder(ObservablePattern* pattern, ITransport* transport,
                                 LoopManager* loopMgr, int patId, int laneId)
    : pattern(pattern), patId_(patId), laneId_(laneId)
{
    // The loop manager before anything reads a phase: the free anchors live there,
    // shared with the editor's own playhead.
    playhead.setLoopManager(loopMgr);
    playhead.setFixedPattern(patId);
    playhead.setTransport(transport, pattern ? pattern->song() : nullptr);

    // Growth and the controller flush ride the playhead's timer rather than one of
    // their own. It already runs at 16-50 ms while the transport rolls, far finer
    // than the one check per bar growth actually needs, and the project has no
    // polling threads to add one to.
    playhead.onTick = [this]() {
        growTick();
        if (!recParams_.empty() &&
            std::chrono::duration<double>(Clock::now() - lastParamFlush_).count() >= kParamFlushSecs)
            flushRecordedParams();
    };
}

PatternRecorder::~PatternRecorder() = default;

const Pattern* PatternRecorder::pat() const
{
    if (!pattern || patId_ <= 0) return nullptr;
    for (const auto& p : pattern->get().patterns)
        if (p.id == patId_) return &p;
    return nullptr;
}

bool PatternRecorder::valid() const
{
    const Pattern* p = pat();
    return p && (p->type == PatternType::PIANOROLL || p->type == PatternType::DRUM);
}

int PatternRecorder::instrumentId() const
{
    const Pattern* p = pat();
    return p ? p->instrumentId : 0;
}

bool PatternRecorder::recordsOnNoteOn() const
{
    const Pattern* p = pat();
    return p && p->type == PatternType::DRUM;
}

// Overdub only: the note is added, never replacing whatever is already there. A
// pianoroll note keeps the length it was played for; a drum hit has no length, so
// lenBeats is not used. Both map straight off the wire — drum maps are keyed by
// MIDI note already, so neither needs the pitch translated.
void PatternRecorder::commitNote(int pitch, float startBeat, float lenBeats, int velocity)
{
    const Pattern* p = pat();
    if (!p || pitch < 0 || pitch > 127) return;
    if (p->type == PatternType::DRUM) {
        pattern->addDrumNote(patId_, pitch, startBeat, velocity / 127.0f);
    } else if (p->type == PatternType::PIANOROLL) {
        if (lenBeats <= 0.0f) return;
        pattern->addNote(patId_, startBeat, pitch, lenBeats, velocity / 127.0f);
    }
}

int PatternRecorder::recordPatternBeats() const
{
    return std::max(1, (int)patternLengthBeats());
}

float PatternRecorder::headBeat()
{
    playhead.setNumCols(recordPatternBeats());
    return playhead.patternBeat(playhead.transportBars());
}

// ── MIDI input and recording ─────────────────────────────────────────────────

void PatternRecorder::setRecordArmed(bool on)
{
    if (on == recArmed_) return;
    recArmed_ = on;
    if (!on) endTake();
}

void PatternRecorder::noteOn(int pitch, int velocity)
{
    if (pitch < 0 || pitch > 127) return;
    if (velocity <= 0) { noteOff(pitch); return; }   // running-status note-off

    float beat = 0.0f;
    if (!beginRecordedEvent(beat)) return;

    if (recordsOnNoteOn()) {
        // No length to work out, but the start is quantised like any other.
        float start = 0.0f, len = 0.0f;
        recordedNoteSpan(beat, beat, start, len);
        commitNote(pitch, start, len, velocity);
        skipLiveEcho(pitch, start);
        return;
    }
    recNotes_.push_back({pitch, beat, velocity});
}

bool PatternRecorder::beginRecordedEvent(float& beat)
{
    if (!recArmed_ || !valid()) return false;

    // Two things have to be true to record, and "nothing happened" looks the same
    // for both, so say which one failed when tracing is on.
    const bool rolling = playhead.transportPlaying();
    beat = rolling ? headBeat() : -1.0f;
    if (luvieDebug() && (!rolling || beat < 0.0f))
        fprintf(stderr, "[luvie] not recorded into pattern %d: %s\n", patId_, !rolling
                ? "transport is not rolling"
                : "the pattern has no position to record at");
    if (!rolling || beat < 0.0f) return false;

    if (!recUndo_) {
        recUndo_.emplace(pattern->song());              // the take starts here
        // Hold on to the phase the take begins with. In Song Mode the pattern stops
        // being played when the playhead leaves its block, and without this the
        // position would jump to the default tiling half way through a take.
        playhead.pinPatternAnchor();
        // And give the take somewhere to live in the song, so what was just played
        // is there in the Song Editor rather than only in the pattern.
        ensureSongBlock();
    }
    // Before the event is placed, so a pattern about to stop looping has already
    // gained the room it may need.
    engageGrow();
    return true;
}

void PatternRecorder::param(const std::string& type, int value)
{
    float beat = 0.0f;
    if (!beginRecordedEvent(beat)) return;

    const Clock::time_point now = Clock::now();
    ParamTouch& t = recParamTouch_[type];
    const bool gestureStart = t.value < 0 ||
        std::chrono::duration<double>(now - t.at).count() > kParamGestureGapSecs;
    t.at = now;
    // A control can repeat itself (some send on a timer); a repeat adds nothing.
    if (!gestureStart && value == t.value) return;
    t.value = value;

    if (recParams_.empty()) lastParamFlush_ = now;   // the batch starts its clock
    recParams_.push_back({type, beat, value, gestureStart});
}

void PatternRecorder::flushRecordedParams()
{
    lastParamFlush_ = Clock::now();
    if (recParams_.empty()) return;
    auto pending = std::move(recParams_);
    recParams_.clear();
    if (!pat()) return;

    // A point this close to the start is the anchor's job: it cannot be deleted or
    // moved, so a value landing on it becomes the anchor's value instead.
    constexpr float kAnchorEps = 1.0f / 256.0f;

    ObservableSong::Batch batch(pattern->song());
    for (const RecordingParam& e : pending) {
        // Look the lane up each time: the pattern is edited as we go.
        const Pattern* p = pat();
        if (!p) return;

        int laneId = -1;
        for (const auto& l : p->paramLanes)
            if (l.type == e.type) { laneId = l.id; break; }
        if (laneId < 0) {
            laneId = pattern->addPatternParamLane(patId_, e.type);
            if (laneId < 0) continue;
            p = pat();   // the vector the lane lives in may have moved
            if (!p) return;
        }
        const ParamLane* lane = nullptr;
        for (const auto& l : p->paramLanes)
            if (l.id == laneId) { lane = &l; break; }
        if (!lane) continue;

        // Touch: within one movement, whatever was on the lane between the previous
        // value and this one is what the player just replaced. If the pattern
        // wrapped in between, that span runs off the end and in again at the start.
        // Messages drained together all read the same playhead position, so the
        // span can be empty: then the value already written there is the one being
        // replaced, rather than two points stacking on one beat.
        ParamTouch& t = recParamTouch_[e.type];
        std::vector<int> doomed;
        int anchorId = -1;
        const bool sameBeat = std::fabs(e.beat - t.beat) < 1e-6f;
        for (const auto& pt : lane->points) {
            if (pt.anchor) { anchorId = pt.id; continue; }
            if (e.gestureStart || t.beat < 0.0f) continue;
            const bool inSpan = sameBeat ? std::fabs(pt.beat - e.beat) < 1e-6f
                              : (e.beat > t.beat) ? (pt.beat > t.beat && pt.beat <= e.beat)
                              : (pt.beat > t.beat || pt.beat <= e.beat);
            if (inSpan) doomed.push_back(pt.id);
        }
        auto& taken = takeParamPoints_[laneId];
        for (int id : doomed) {
            pattern->removeParamPoint(id);
            taken.erase(id);
        }
        // A movement carried across the loop point passes over the anchor too, so it
        // takes the value the control had when it came round.
        const bool wrapped = !e.gestureStart && t.beat >= 0.0f && !sameBeat && e.beat < t.beat;
        t.beat = e.beat;

        if (anchorId >= 0 && (wrapped || e.beat < kAnchorEps))
            pattern->moveParamPoint(anchorId, 0.0f, e.value);
        if (e.beat < kAnchorEps) continue;
        const int id = pattern->addPatternParamPoint(patId_, laneId, e.beat, e.value);
        if (id >= 0) taken.insert(id);
    }
}

void PatternRecorder::thinRecordedParams()
{
    if (takeParamPoints_.empty()) return;
    auto taken = std::move(takeParamPoints_);
    takeParamPoints_.clear();

    const Pattern* p = pat();
    if (!p) return;

    // Only runs of points this take wrote are thinned; anything the take left
    // alone, and the ends of each run, stay exactly where they were.
    std::vector<int> drop;
    std::vector<float> beats;
    std::vector<int>   values, ids;
    std::vector<char>  keep;
    for (const auto& lane : p->paramLanes) {
        auto it = taken.find(lane.id);
        if (it == taken.end()) continue;
        const double tol = pattern->get().paramMax(p->instrumentId, lane.type) / 100.0;   // ~1 CC step
        auto flushRun = [&]() {
            if (ids.size() > 2) {
                thinParamRun(beats, values, tol, keep);
                for (size_t i = 0; i < ids.size(); i++)
                    if (!keep[i]) drop.push_back(ids[i]);
            }
            beats.clear(); values.clear(); ids.clear();
        };
        for (const auto& pt : lane.points) {
            if (pt.anchor || !it->second.count(pt.id)) { flushRun(); continue; }
            beats.push_back(pt.beat);
            values.push_back(pt.value);
            ids.push_back(pt.id);
        }
        flushRun();
    }
    if (luvieDebug() && !drop.empty())
        fprintf(stderr, "[luvie] automation thinned: %zu points dropped\n", drop.size());
    if (drop.empty()) return;
    ObservableSong::Batch batch(pattern->song());
    for (int id : drop) pattern->removeParamPoint(id);
}

float PatternRecorder::snapBeats() const
{
    const Pattern* p = pat();
    return p ? patternSnapBeats(*p) : 0.0f;
}

float PatternRecorder::quantiseBeat(float beat) const
{
    const float q = snapBeats();
    if (q <= 0.0f) return beat;            // Snap off: play it as played
    return std::round(beat / q) * q;
}

void PatternRecorder::skipLiveEcho(int pitch, float startBeat)
{
    // Only rounding moves a note forwards: unquantised, it lands where the head was.
    const float q = snapBeats();
    if (q <= 0.0f || !onSkipNoteOnce) return;
    playhead.setNumCols(recordPatternBeats());
    const float bars = playhead.transportBars();
    const Playhead::PatternPos pp = playhead.patternPos(bars);
    const float cur = playhead.patternBeat(bars);
    if (!pp.running || pp.beatsPerBar <= 0.0f || cur < 0.0f) return;
    // Rounding to the nearest division moves a start forwards by at most half of
    // one; anything further ahead is not this note's echo.
    const float ahead = startBeat - cur;
    if (ahead <= 0.0f || ahead > q) return;
    // A quarter of a division either side: loose enough for the float position the
    // head is read at, tight enough never to reach a neighbouring grid line.
    onSkipNoteOnce(instrumentId(), pitch,
                   (double)bars + ahead / pp.beatsPerBar,
                   0.25 * q / pp.beatsPerBar);
}

void PatternRecorder::recordedNoteSpan(float rawStart, float rawEnd,
                                       float& startBeat, float& lenBeats) const
{
    const float total = (float)recordPatternBeats();
    const float q     = snapBeats();

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

void PatternRecorder::noteOff(int pitch)
{
    for (int i = (int)recNotes_.size() - 1; i >= 0; i--) {
        if (recNotes_[i].pitch != pitch) continue;
        const RecordingNote n = recNotes_[i];
        recNotes_.erase(recNotes_.begin() + i);

        const float end = headBeat();
        if (end >= 0.0f) {
            float start = 0.0f, len = 0.0f;
            recordedNoteSpan(n.startBeat, end, start, len);
            commitNote(n.pitch, start, len, n.velocity);
            skipLiveEcho(n.pitch, start);
        }
        break;   // one note-off releases one note-on
    }
}

void PatternRecorder::endTake()
{
    // headBeat() is still meaningful here, and a key the user is holding when they
    // disarm is a note they played.
    if (!recNotes_.empty()) {
        auto pending = std::move(recNotes_);
        recNotes_.clear();
        const float end = headBeat();
        if (end >= 0.0f) {
            for (const auto& n : pending) {
                float start = 0.0f, len = 0.0f;
                recordedNoteSpan(n.startBeat, end, start, len);
                commitNote(n.pitch, start, len, n.velocity);
            }
        }
    }
    // Controller values still buffered belong to this take, and the thinning has to
    // see all of them. Both inside the take's undo group, so one ctrl-Z undoes the
    // notes and the automation together.
    flushRecordedParams();
    thinRecordedParams();
    recParamTouch_.clear();

    // Inside the still-open undo group, and after the commits above: the trim has to
    // see the notes this take just wrote, and the bars it removes belong to the same
    // ctrl-Z as the notes that caused them.
    endGrow();
    placedForTake_ = false;
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

float PatternRecorder::patternBarBeats() const
{
    const Pattern* p = pat();
    return p ? (float)std::max(1, p->timeSigTop) : 0.0f;
}

float PatternRecorder::patternLengthBeats() const
{
    const Pattern* p = pat();
    return p ? p->lengthBeats : 0.0f;
}

const Lane* PatternRecorder::recordLane() const
{
    // A block always plays the pattern of the lane it sits in, so the lane is the
    // one that shows this pattern — never simply the one that was selected, which
    // may belong to another pattern and would have two recorders fighting over one
    // lane. The lane armed from is preferred only if it really is this pattern's.
    if (!pattern) return nullptr;
    const Lane* first = nullptr;
    for (const auto& track : pattern->get().tracks)
        for (const auto& l : track.lanes) {
            if (l.patternId != patId_) continue;
            if (l.id == laneId_) return &l;
            if (!first) first = &l;
        }
    return first;
}

const PatternInstance*
PatternRecorder::growableInstance(float bars, const Playhead::PatternPos& pp) const
{
    const Pattern* p = pat();
    if (!p) return nullptr;

    const Lane* lane = recordLane();
    if (!lane) return nullptr;

    for (const auto& inst : lane->patterns) {
        if (inst.patternId != patId_) continue;
        if (bars < inst.startBar || bars >= inst.startBar + inst.length) continue;
        // LoopManager::sync() reads the time signature at the block's start bar while
        // the playhead reads it at the current one. Across a time-signature marker
        // those disagree, and every conversion below would then be measured in one
        // unit and applied in the other — so leave such a take alone.
        const float instBpb = pattern->song()->patternBeatsPerBar((int)inst.startBar, *p);
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

void PatternRecorder::ensureSongBlock()
{
    const Pattern* p = pat();
    if (!p) return;

    const float bars = playhead.transportBars();
    Playhead::PatternPos pp = playhead.patternPos(bars);
    if (!pp.running || pp.beatsPerBar <= 0.0f) return;
    // Loop Mode, and a Loop-Editor switch layered over Song Mode, are both the user
    // working on the pattern rather than on the song. Nothing gets placed.
    if (playhead.isLoopActive() || pp.manualLoop) return;

    float len = patternLengthBeats();
    const float bar = patternBarBeats();
    if (len <= 0.0f || bar <= 0.0f) return;

    const Lane* lane = recordLane();
    if (!lane) return;
    const int laneId = lane->id;

    // Already placed here — including the case that put the pattern under the
    // playhead in the first place, which is what made it active.
    for (const auto& inst : lane->patterns)
        if (bars >= inst.startBar && bars < inst.startBar + inst.length) return;

    // Where the block starts. When nothing is playing the pattern its phase is
    // ours, so the take keeps everything from the bar the transport was started
    // in: the pattern is re-anchored there, and the block runs from there. When
    // something else is playing it (a block of it elsewhere), or the start is not
    // known, the pass the playhead is in — whole passes may have gone by if the
    // pattern has been tiling the song since its anchor.
    float anchor = 0.0f;
    const float runBar = std::floor(runStartBar_ + 1.0e-4f);
    if (pp.free && runStartBar_ >= 0.0f && runBar <= bars) {
        anchor = runBar;
        if (std::fabs(anchor - pp.anchorBar) > 1.0e-6f) {
            playhead.reanchorPattern(anchor);
            pp = playhead.patternPos(bars);
        }
    } else {
        const int cycle = (int)std::floor(pp.elapsedBeats / len);
        anchor = pp.anchorBar + (float)cycle * len / pp.beatsPerBar;
    }
    if (anchor < 0.0f) return;
    const float elapsed = (bars - anchor) * pp.beatsPerBar;   // beats since `anchor`

    // The song editor does not allow two blocks to overlap in a lane, so neither
    // will we. Nothing sits on the playhead (checked above), so a neighbour is
    // either wholly before it, which pushes the start later, or wholly after, which
    // bounds the end.
    float start   = anchor;
    float nextBar = -1.0f;
    for (const auto& inst : lane->patterns) {
        const float iEnd = inst.startBar + inst.length;
        if (iEnd <= bars && iEnd > start) start = iEnd;
        if (inst.startBar > bars && (nextBar < 0.0f || inst.startBar < nextBar))
            nextBar = inst.startBar;
    }

    // With Grow armed the take is a single pass that starts at the anchor, so the
    // pattern is lengthened now to reach past the playhead — the bars before the
    // first note are part of it — and growth carries on from there. Without it the
    // block repeats the pattern up to the end of the pass the playhead is in.
    const float origLen = len;
    if (growArmed_) {
        float want = (float)((int)std::floor(elapsed / bar) + 2) * bar;
        want = std::min(want, (float)kGrowMaxBars * bar);
        if (nextBar >= 0.0f) want = std::min(want, (nextBar - anchor) * pp.beatsPerBar);
        if (want > len + kGrowEps && elapsed < want) {
            pattern->setPatternLength(patId_, want);
            len = want;
        }
    }
    const int   passes = std::max(1, (int)std::floor(elapsed / len) + 1);
    float       end    = anchor + (float)passes * len / pp.beatsPerBar;
    if (nextBar >= 0.0f) end = std::min(end, nextBar);
    if (end <= bars + 1.0e-4f || end <= start) {
        if (luvieDebug())
            fprintf(stderr, "[luvie] no block placed for pattern %d: neighbouring "
                    "blocks leave no room around bar %.2f\n", patId_, bars);
        return;
    }
    // A start pushed later by a neighbour keeps the anchor's phase: the block opens
    // part way into the pattern.
    const float offset = std::fmod((start - anchor) * pp.beatsPerBar, len);

    const int instId = pattern->song()->placePattern(laneId, patId_, start, end - start, offset);
    if (!instId) return;
    if (luvieDebug())
        fprintf(stderr, "[luvie] placed a block of pattern %d for the take at bar %.2f "
                "(%.2f bars)\n", patId_, start, end - start);
    // The trim floors for a block made for the take: the pattern goes back no
    // shorter than it was, and the block no longer than the pattern needs.
    placedBaseBeats_ = origLen;
    placedForTake_   = true;
    // Line the pattern's phase up with the block just made. LoopManager::sync() will
    // derive exactly this anchor on its next tick; doing it here means the beats
    // recorded between now and then are measured against the same origin, so nothing
    // shifts when the song takes the pattern over.
    playhead.reanchorPattern(anchor);
}

void PatternRecorder::setGrowArmed(bool on)
{
    if (on == growArmed_) return;
    growArmed_ = on;
    // Switching it off stops the growth where it is and trims what was not played
    // into, rather than leaving a take growing that the user has said to stop.
    if (!on) endGrow();
}

void PatternRecorder::engageGrow()
{
    if (!growArmed_ || growActive_ || !pat()) return;

    const float bars = playhead.transportBars();
    const Playhead::PatternPos pp = playhead.patternPos(bars);
    if (!pp.running || pp.beatsPerBar <= 0.0f) return;

    const float len = patternLengthBeats();
    const float bar = patternBarBeats();
    if (len <= 0.0f || bar <= 0.0f) return;

    const int cycle = (int)std::floor(pp.elapsedBeats / len);

    // A song block playing the pattern owns its phase — the engine reads the block,
    // not the LoopManager — so that is what has to grow. ensureSongBlock() has
    // normally just placed one for the take; when there is none to be had, the
    // pattern is running on its own anchor and we move that instead.
    const bool loopPhase = playhead.isLoopActive() || pp.manualLoop;
    const PatternInstance* inst = loopPhase ? nullptr : growableInstance(bars, pp);

    if (inst) {
        if (cycle > 0) {
            // The block is several passes long and we came in after the first. The
            // pass in progress cannot be extended without moving what is already
            // playing, and the anchor is not ours to move here. Record normally.
            if (luvieDebug())
                fprintf(stderr, "[luvie] not growing: the block is longer than the "
                        "pattern and the playhead is past its first pass - start the "
                        "take in the first pass\n");
            return;
        }
        growInstId_         = inst->id;
        // A block placed for this take was sized to the pattern; it trims with it.
        growBaseInstLength_ = placedForTake_ ? 0.0f : inst->length;
    } else if (loopPhase || pp.free) {
        // Nothing is playing it from a block: Loop Mode, a Loop-Editor switch, or a
        // pattern with no placement at all. The anchor is ours.
        growInstId_         = 0;
        growBaseInstLength_ = 0.0f;
        // Advance it by the whole passes already played. That is an exact multiple of
        // the pattern length, so it maps the firing set onto itself: nothing moves
        // and nothing is heard, but the pass in progress becomes pass 0 and growing
        // the pattern now extends it.
        if (cycle > 0)
            playhead.reanchorPattern(pp.anchorBar + (float)cycle * len / pp.beatsPerBar);
    } else {
        // A block is playing it, but not one we can work with — a time signature
        // change inside it, or the same pattern on another lane supplying the phase.
        if (luvieDebug())
            fprintf(stderr, "[luvie] not growing: no usable pattern block under the "
                    "playhead on the pattern's lane\n");
        return;
    }

    // The length before the take, even if placing its block has already grown it.
    growBaseBeats_ = placedForTake_ ? std::min(len, placedBaseBeats_) : len;
    growActive_    = true;
    growTick();     // apply the invariant now, not one tick from now
}

void PatternRecorder::growTick()
{
    if (!growActive_ || !pat()) return;
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
        pattern->setPatternLength(patId_, want);
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

    if (onGrew) onGrew(patId_, pp.elapsedBeats);
}

void PatternRecorder::endGrow()
{
    if (!growActive_) return;
    growActive_ = false;
    const int   instId      = growInstId_;
    const float baseBeats   = growBaseBeats_;
    const float baseInstLen = growBaseInstLength_;
    growInstId_ = 0;

    const Pattern* p = pat();
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
    pattern->setPatternLength(patId_, finalBeats);
    if (instId) {
        const PatternInstance* inst = pattern->song()->instanceById(instId);
        if (!inst) return;
        const Pattern* q = pat();
        if (!q) return;
        const float bpb = pattern->song()->patternBeatsPerBar((int)inst->startBar, *q);
        if (bpb <= 0.0f) return;
        const float anchor = inst->startBar - inst->startOffset / bpb;
        const float want   = std::max(baseInstLen,
                                      anchor + finalBeats / bpb - inst->startBar);
        if (want < inst->length) pattern->song()->resizePattern(instId, want);
    }
}

// ── PatternRecorders ─────────────────────────────────────────────────────────

PatternRecorders::~PatternRecorders() = default;

void PatternRecorders::setContext(ObservablePattern* pat, ITransport* t, LoopManager* lm)
{
    pattern   = pat;
    transport = t;
    loopMgr   = lm;
}

PatternRecorder* PatternRecorders::find(int patId) const
{
    for (const auto& r : recs)
        if (r->patternId() == patId) return r.get();
    return nullptr;
}

PatternRecorder& PatternRecorders::obtain(int patId, int laneId)
{
    if (PatternRecorder* r = find(patId)) {
        r->setLaneId(laneId);
        return *r;
    }
    recs.push_back(std::make_unique<PatternRecorder>(pattern, transport, loopMgr, patId, laneId));
    PatternRecorder& r = *recs.back();
    r.setLoopMode(loopMode);
    r.setRunStart(runStart);
    r.onSkipNoteOnce = [this](int instr, int pitch, double bar, double tol) {
        if (onSkipNoteOnce) onSkipNoteOnce(instr, pitch, bar, tol);
    };
    r.onGrew = [this](int id, float beat) { if (onGrew) onGrew(id, beat); };
    return r;
}

void PatternRecorders::release(int patId)
{
    std::erase_if(recs, [patId](const std::unique_ptr<PatternRecorder>& r) {
        return r->patternId() == patId && !r->recordArmed() && !r->growArmed();
    });
}

bool PatternRecorders::recordArmed(int patId) const
{
    const PatternRecorder* r = find(patId);
    return r && r->recordArmed();
}

bool PatternRecorders::growArmed(int patId) const
{
    const PatternRecorder* r = find(patId);
    return r && r->growArmed();
}

bool PatternRecorders::anyRecordArmed() const
{
    for (const auto& r : recs)
        if (r->recordArmed() && r->valid()) return true;
    return false;
}

void PatternRecorders::setRecordArmed(int patId, int laneId, bool on)
{
    if (patId <= 0) return;
    if (on) { obtain(patId, laneId).setRecordArmed(true); return; }
    if (PatternRecorder* r = find(patId)) r->setRecordArmed(false);
    release(patId);
}

void PatternRecorders::setGrowArmed(int patId, int laneId, bool on)
{
    if (patId <= 0) return;
    if (on) { obtain(patId, laneId).setGrowArmed(true); return; }
    if (PatternRecorder* r = find(patId)) r->setGrowArmed(false);
    release(patId);
}

bool PatternRecorders::disarmAllRecord()
{
    bool any = false;
    for (const auto& r : recs) {
        if (!r->recordArmed()) continue;
        r->setRecordArmed(false);
        any = true;
    }
    std::erase_if(recs, [](const std::unique_ptr<PatternRecorder>& r) {
        return !r->recordArmed() && !r->growArmed();
    });
    return any;
}

void PatternRecorders::clear()
{
    // Discarded, not ended: by now the song holds the new project, and committing
    // a held note or trimming a grown pattern would write into whatever pattern
    // there happens to have the same id.
    recs.clear();
}

void PatternRecorders::forEachRecording(const std::function<void(PatternRecorder&)>& fn)
{
    // Dropped patterns go first: nothing can record into them, and a recorder whose
    // pattern id is later reused must not start recording into its stranger.
    std::erase_if(recs, [](const std::unique_ptr<PatternRecorder>& r) { return !r->valid(); });
    for (const auto& r : recs)
        if (r->recordArmed()) fn(*r);
}

void PatternRecorders::setRunStart(float bar)
{
    runStart = bar;
    for (const auto& r : recs) r->setRunStart(bar);
}

void PatternRecorders::setLoopMode(bool loop)
{
    loopMode = loop;
    for (const auto& r : recs) r->setLoopMode(loop);
}
