// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "sequencer.hpp"
#include "chords.hpp"
#include "loopFiring.hpp"
#include "paramLaneTypes.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

// ── Construction / destruction ────────────────────────────────────────────────

Sequencer::Sequencer()
{
    // Pre-reserve every RT-thread scratch buffer so renderCycle() never allocates,
    // not even on its first call. The bounds match the worst dense window a JACK
    // cycle (up to ~90 ms at 4096 frames @ 44.1 kHz) can produce; param-ramp
    // densification is capped per segment. Exceeding a bound would reallocate (an
    // RT-safety violation) but would not crash.
    activeNotes.reserve(2048);
    paramScratch.reserve(4096);
    resetScratch.reserve(64);
}

Sequencer::~Sequencer()
{
    if (loopMgr)      loopMgr->removeObserver(this);
    if (timeline) timeline->removeObserver(this);
}

// ── Owner-thread setters ──────────────────────────────────────────────────────

void Sequencer::setTimeline(ObservableSong* tl)
{
    if (timeline) timeline->removeObserver(this);
    timeline = tl;
    if (timeline) timeline->addObserver(this);
    rebuildSnapshot();
}

void Sequencer::setLoopManager(LoopManager* a)
{
    if (loopMgr) loopMgr->removeObserver(this);
    loopMgr = a;
    if (loopMgr) loopMgr->addObserver(this);
    rebuildSnapshot();
}

void Sequencer::setInstruments(const std::vector<InstrumentRouting>& routings)
{
    instrumentMap_.clear();
    for (const auto& r : routings)
        instrumentMap_[r.instrumentId] = r;
    rebuildSnapshot();
}

void Sequencer::setLoopMode(bool mode)
{
    // Entering a mode supersedes a hand-off out of one that has not landed yet — the
    // user clicked back into Loop Mode mid-transition.
    cancelHandoff();
    loopMode = mode;
    rebuildSnapshot();
}

void Sequencer::endLoopMode(float resumeBar)
{
    loopMode = false;
    Snapshot newSnap;
    if (!buildSnapshot(newSnap)) return;

    // Park the song content next to the live one and arm the switch. `snap` is left
    // alone: the loops must keep playing until the RT thread reaches the seam, and it
    // is the one that decides where that is (see the header). Both halves go under the
    // same lock, so no cycle can see one without the other.
    std::lock_guard<std::mutex> lock(snapMutex);
    pendingSnap      = std::move(newSnap);
    pendingHandoff   = true;
    pendingResumeBar = resumeBar;
}

void Sequencer::cancelHandoff()
{
    std::lock_guard<std::mutex> lock(snapMutex);
    pendingHandoff = false;
}

void Sequencer::swapSnapshots()
{
    snap.segs.swap      (pendingSnap.segs);
    snap.tracks.swap    (pendingSnap.tracks);
    snap.paramInsts.swap(pendingSnap.paramInsts);
    std::swap(snap.loopMode, pendingSnap.loopMode);
}

void Sequencer::setSongLoop(bool enabled, float startBar, float endBar)
{
    // Store the region before flipping the flag on so the RT thread never sees a
    // stale region with a fresh "enabled". A rare one-cycle mismatch the other way
    // (region updated while enabled stays true) is self-correcting.
    songLoopStart.store(startBar, std::memory_order_relaxed);
    songLoopEnd.store(endBar,     std::memory_order_relaxed);
    songLoopOn.store(enabled,     std::memory_order_relaxed);
}

// ── Snapshot building (owner thread) ──────────────────────────────────────────

void Sequencer::rebuildSnapshot()
{
    if (rebuildsSuspended) return;
    Snapshot newSnap;
    if (!buildSnapshot(newSnap)) return;
    std::lock_guard<std::mutex> lock(snapMutex);
    // While a hand-off is armed the loops are still the thing playing, so an edit
    // arriving in the meantime belongs to the snapshot the switch will land on.
    if (pendingHandoff) pendingSnap = std::move(newSnap);
    else                snap        = std::move(newSnap);
}

// Returns false if there is nothing to publish, in which case the caller must leave
// the live snapshot alone rather than commit an empty one.
bool Sequencer::buildSnapshot(Snapshot& newSnap)
{
    if (!timeline) return false;

    const Timeline& tl = timeline->get();
    newSnap.loopMode = loopMode;

    // The tempo table is built by the data model, so the RT thread and the UI
    // cannot disagree about where a bar falls in time.
    newSnap.segs = timeline->tempoMap();

    // Build per-track note data.
    auto buildNotes = [&](InstanceSnap& is, const Pattern* pat, int trackIdx, int trackInstrument) {
        is.portName    = "";
        is.midiChannel = trackIdx % 16;
        // A pattern's own instrument is authoritative; fall back to the track's
        // instrument when unset (0) so patterns placed on a track still route.
        int instrId = pat->instrumentId != 0 ? pat->instrumentId : trackInstrument;
        if (instrId != 0) {
            auto it = instrumentMap_.find(instrId);
            if (it != instrumentMap_.end()) {
                is.portName    = it->second.portName;
                is.midiChannel = it->second.midiChannel - 1;
            }
        }
        if (pat->type == PatternType::DRUM) {
            bool anySolo = !pat->drumSolo.empty();
            for (const DrumNote& dn : pat->drumNotes) {
                bool isSolo = pat->drumSolo.count(dn.note) > 0;
                bool isMute = pat->drumMute.count(dn.note) > 0;
                if (isMute || (anySolo && !isSolo)) continue;
                int midi = std::clamp(dn.note, 0, 127);
                is.notes.push_back({midi, dn.beat, drumNoteLen, dn.velocity});
            }
        } else {
            int chordIndex = chordIndexForHash(pat->chordHash);
            for (const Note& note : pat->notes)
                is.notes.push_back({patternNoteMidi(*pat, note, chordIndex),
                                    note.beat, note.length, note.velocity});
        }
    };

    // Pre-compute all firing events (point values + half-integer crossings) for a lane.
    auto buildParamEvents = [](const ParamLane& lane) -> std::vector<ParamEventSnap> {
        std::vector<ParamEventSnap> evts;
        auto sink = [&](float beat, int value) { evts.push_back({beat, value}); };
        for (int i = 0; i < (int)lane.points.size(); i++) {
            evts.push_back({lane.points[i].beat, lane.points[i].value});
            if (i + 1 < (int)lane.points.size())
                densifyParamRamp(lane.points[i].beat,  lane.points[i+1].beat,
                                 lane.points[i].value, lane.points[i+1].value, sink);
        }
        std::sort(evts.begin(), evts.end(),
                  [](const ParamEventSnap& a, const ParamEventSnap& b) { return a.beat < b.beat; });
        return evts;
    };

    bool anySolo = std::any_of(tl.tracks.begin(), tl.tracks.end(),
                               [](const Track& t) { return t.solo; });

    auto findPattern = [&](int patId) -> const Pattern* {
        for (const auto& p : tl.patterns)
            if (p.id == patId) return &p;
        return nullptr;
    };

    // Build a forever-looping instance (+ its param lanes) for an active pattern
    // and append it to `ts`. Shared by loop mode and by manual Loop-Editor
    // switches layered over song mode, so both funnel through LoopManager.
    auto emitLoopInstance = [&](TrackSnap& ts, const Pattern* pat, float anchorBar,
                                int trackIdx, int trackInstrument) {
        if (!pat || pat->lengthBeats <= 0.0f) return;
        InstanceSnap is;
        is.startBar     = anchorBar;
        is.length       = 1.0e9f;
        is.startOffset  = 0.0f;
        is.patternBeats = pat->lengthBeats;
        is.loop         = true;
        float beatsPerBar = timeline->patternBeatsPerBar((int)std::max(0.0f, anchorBar), *pat);
        is.beatsPerBar    = beatsPerBar;
        buildNotes(is, pat, trackIdx, trackInstrument);

        // Param lanes. Build BEFORE moving `is` below — moving leaves portName empty.
        for (const auto& lane : pat->paramLanes) {
            auto evts = buildParamEvents(lane);
            if (evts.empty()) continue;
            ParamInstSnap pis;
            pis.startBar     = anchorBar;
            pis.length       = 1.0e9f;
            pis.startOffset  = 0.0f;
            pis.beatsPerBar  = beatsPerBar;
            pis.patternBeats = pat->lengthBeats;
            pis.loop         = true;
            pis.portName     = is.portName;
            pis.midiChannel  = is.midiChannel;
            pis.priority     = trackIdx + 1;
            pis.ccNumber     = ccForType(lane.type);
            pis.events       = std::move(evts);
            newSnap.paramInsts.push_back(std::move(pis));
        }

        if (!is.notes.empty())
            ts.instances.push_back(std::move(is));
    };

    if (loopMode) {
        if (!loopMgr) return false;
        const auto& actives = loopMgr->patterns();
        int trackIdx = 0;
        for (const Track& track : tl.tracks) {
            TrackSnap ts;
            // Each stacked lane loops its own displayed pattern independently when
            // that pattern is active; layer them all on the track's instrument.
            if (!track.mute && (!anySolo || track.solo))
            for (const Lane& laneRef : track.lanes) {
                auto it = actives.find(laneRef.patternId);
                if (it == actives.end()) continue;
                emitLoopInstance(ts, findPattern(laneRef.patternId), it->second,
                                 trackIdx, track.instrumentId);
            }
            newSnap.tracks.push_back(std::move(ts));
            ++trackIdx;
        }
    } else {
        int trackIdx = 0;
        for (const Track& track : tl.tracks) {
            TrackSnap ts;
            if (track.mute || (anySolo && !track.solo)) {
                newSnap.tracks.push_back(std::move(ts));
                ++trackIdx;
                continue;
            }
            if (track.lanes.empty()) { newSnap.tracks.push_back(std::move(ts)); ++trackIdx; continue; }
            // Iterate every lane: a track can have stacked lanes that layer
            // independent patterns on the same instrument simultaneously.
            for (const Lane& lane : track.lanes)
            for (const PatternInstance& inst : lane.patterns) {
                const Pattern* pat = timeline->patternForInstance(inst.id);
                if (!pat || pat->lengthBeats <= 0.0f) continue;

                // Defer to the LoopManager (LoopManager): a manual disable
                // silences this pattern's song instances for the current placement,
                // and a manual loop of the same pattern takes over entirely, so the
                // two never double-trigger. Timeline placement still drives timing.
                if (loopMgr && (loopMgr->isManuallyDisabled(inst.patternId) ||
                            loopMgr->isManual(inst.patternId)))
                    continue;

                InstanceSnap is;
                is.startBar     = inst.startBar;
                is.length       = inst.length;
                is.startOffset  = inst.startOffset;
                is.patternBeats = pat->lengthBeats;
                float beatsPerBar = timeline->patternBeatsPerBar((int)inst.startBar, *pat);
                is.beatsPerBar    = beatsPerBar;
                buildNotes(is, pat, trackIdx, track.instrumentId);

                // Param lanes for this pattern instance. Build BEFORE moving `is`
                // into ts.instances below — moving leaves is.portName empty.
                if (!is.portName.empty()) {
                    for (const auto& lane : pat->paramLanes) {
                        auto evts = buildParamEvents(lane);
                        if (evts.empty()) continue;
                        ParamInstSnap pis;
                        pis.startBar     = inst.startBar;
                        pis.length       = inst.length;
                        pis.startOffset  = inst.startOffset;
                        pis.beatsPerBar  = beatsPerBar;
                        pis.patternBeats = pat->lengthBeats;
                        pis.portName     = is.portName;
                        pis.midiChannel  = is.midiChannel;
                        pis.priority     = trackIdx + 1;
                        pis.ccNumber     = ccForType(lane.type);
                        pis.events       = std::move(evts);
                        newSnap.paramInsts.push_back(std::move(pis));
                    }
                }
                ts.instances.push_back(std::move(is));
            }

            // Manual Loop-Editor switches layer forever-looping patterns on top of
            // song playback, funnelled through the same LoopManager the soft
            // playhead reads. Song-originated actives are already covered by the
            // timeline instances above, so only manual ones are added here.
            if (loopMgr)
                for (const Lane& lane : track.lanes) {
                    if (!loopMgr->isManual(lane.patternId)) continue;
                    auto it = loopMgr->patterns().find(lane.patternId);
                    if (it == loopMgr->patterns().end()) continue;
                    emitLoopInstance(ts, findPattern(lane.patternId), it->second,
                                     trackIdx, track.instrumentId);
                }

            newSnap.tracks.push_back(std::move(ts));
            ++trackIdx;
        }

        // Song-level param lanes: each routes only to its owning instrument's port,
        // priority 0 (lowest). Beat field stores bar position (1 unit = 1 bar).
        for (const auto& lane : tl.paramLanes) {
            if (lane.instrumentId == 0) continue;
            auto rit = instrumentMap_.find(lane.instrumentId);
            if (rit == instrumentMap_.end() || rit->second.portName.empty()) continue;
            auto evts = buildParamEvents(lane);
            if (evts.empty()) continue;
            ParamInstSnap pis;
            pis.startBar     = 0.0f;
            pis.length       = 1.0e9f;
            pis.startOffset  = 0.0f;
            pis.beatsPerBar  = 1.0f;  // 1 beat = 1 bar at song level
            pis.patternBeats = 0.0f;  // sentinel: song-level, no loop
            pis.portName     = rit->second.portName;
            pis.midiChannel  = rit->second.midiChannel - 1;  // store 0-based
            pis.priority     = 0;
            pis.ccNumber     = ccForType(lane.type);
            pis.events       = std::move(evts);
            newSnap.paramInsts.push_back(std::move(pis));
        }
    }

    return true;
}

// ── RT-safe bar/seconds conversion ────────────────────────────────────────────

double Sequencer::snapBarToSeconds(double bar) const
{
    return timeSettings::mapBarToSeconds(snap.segs, bar);
}

double Sequencer::snapSecondsToBar(double secs) const
{
    return timeSettings::mapSecondsToBar(snap.segs, secs);
}

// ── Cycle rendering (RT thread) ───────────────────────────────────────────────

// Seconds from the current cycle's first frame to musical position `bar`, in the
// active sub-segment's time base. A plain snapBarToSeconds(bar) - cycleStart would
// be wrong for a segment rendered after a loop wrap (its bars map to an *earlier*
// wall-clock than the cycle start); segCycleStartSecs / segMusicalStart carry the
// per-segment offset so the arithmetic stays correct across the seam.
long Sequencer::segFrameOffset(double bar) const
{
    double secs = segCycleStartSecs
                + snapBarToSeconds(bar             - emitBarOffset)
                - snapBarToSeconds(segMusicalStart - emitBarOffset);
    return (long)std::llround(secs * sampleRateHz);
}

bool Sequencer::handoffPoint(double prevBars, double curBars, double& at) const
{
    // Bars are a uniform domain (1.0 == one bar whatever the time signature), so the
    // crossing is just the next position with the resume bar's fractional part.
    const double ph   = (double)pendingResumeBar - std::floor((double)pendingResumeBar);
    double       cand = std::floor(prevBars) + ph;
    if (cand < prevBars) cand += 1.0;
    if (cand >= curBars) return false;
    at = cand;
    return true;
}

bool Sequencer::renderCycle(bool nowPlaying, bool jumped,
                            double cycleStartSecs, double cycleEndSecs)
{
    // Hold the snapshot for the whole cycle — including every snapBarToSeconds()
    // call below and in emit(). try_lock keeps the RT thread from blocking on the
    // owner thread's rebuild; on failure the caller leaves wasPlaying untouched so
    // the missed stop/jump is retried next cycle.
    if (!snapMutex.try_lock()) {
        // The musical cursor did not advance this cycle, so it now trails the clock.
        // Drop it and let the next readable cycle re-sync from the clock, otherwise
        // the lost window would become a permanent lag rather than one skipped one.
        loopCursorValid = false;
        return false;
    }

    const double dtSecs = cycleEndSecs - cycleStartSecs;

    double curBarOffset = barOffset.load(std::memory_order_relaxed);
    emitBarOffset = curBarOffset;

    double linPrev = snapSecondsToBar(cycleStartSecs) + curBarOffset;
    double linCur  = snapSecondsToBar(cycleEndSecs)   + curBarOffset;

    // A transport break resets controllers; a purely musical one (the hand-off below,
    // and the loop seam) only releases what is still held.
    Reset cycleReset = (jumped || (!nowPlaying && wasPlaying)) ? Reset::Hard
                                                              : Reset::None;

    // Seconds from this cycle's first frame to where the window rendered below starts.
    // Non-zero only when a hand-off has already consumed the head of the cycle.
    double baseSegOff = 0.0;

    // ── Loop -> Song hand-off ─────────────────────────────────────────────────
    // Armed by endLoopMode(); this is where it lands. Waiting for the matching bar
    // phase here rather than applying it on arrival is what keeps the switch on the
    // beat however long the message took to reach us.
    if (pendingHandoff) {
        double at = 0.0;
        // Stopped or already jumping: there is no phase to align to, so take it now.
        const bool immediate = !nowPlaying || jumped;
        if (immediate) at = linPrev;

        if (immediate || handoffPoint(linPrev, linCur, at)) {
            if (!immediate) {
                // Play the loops out to the seam first, from the live snapshot.
                segCycleStartSecs = 0.0;
                segMusicalStart   = linPrev;
                renderWindowLocked(true, cycleReset, linPrev, at);
                cycleReset = Reset::None;          // consumed by the pre-seam window
                baseSegOff = snapBarToSeconds(at      - curBarOffset)
                           - snapBarToSeconds(linPrev - curBarOffset);
                if (baseSegOff < 0.0) baseSegOff = 0.0;
            }

            // Release what the loops still hold, exactly at the seam. A musical break,
            // so notes off and nothing else — unless the transport broke too.
            const Reset seamReset = cycleReset == Reset::Hard ? Reset::Hard : Reset::Soft;
            segCycleStartSecs = baseSegOff;
            segMusicalStart   = at;
            renderWindowLocked(nowPlaying, seamReset, at, at);
            cycleReset = Reset::None;

            // Adopt the song content and the re-anchored bar mapping together. The
            // offset moves by a whole number of bars (that is what handoffPoint
            // guarantees), so the beat lands exactly where the loops left it.
            swapSnapshots();
            pendingHandoff  = false;
            curBarOffset   += (double)pendingResumeBar - at;
            barOffset.store(curBarOffset, std::memory_order_relaxed);
            emitBarOffset   = curBarOffset;
            loopCursorValid = false;

            linPrev = pendingResumeBar;
            linCur  = snapSecondsToBar(cycleEndSecs) + curBarOffset;
        }
    }

    const float ls         = songLoopStart.load(std::memory_order_relaxed);
    const float le         = songLoopEnd.load(std::memory_order_relaxed);
    // The region belongs to the song timeline: in Loop mode the patterns free-run
    // against a linear clock, so wrapping there would restart their phase. snap
    // carries the mode so the gate flips in step with the content.
    const bool  haveRegion = songLoopOn.load(std::memory_order_relaxed)
                             && !snap.loopMode
                             && (le - ls) > 1.0e-4f;

    if (!haveRegion || !nowPlaying) {
        // Straight through. Keep the wrapped-position publisher sensible even when
        // the toggle is on but playback has not yet crossed the loop end.
        segCycleStartSecs = baseSegOff;
        segMusicalStart   = linPrev;
        renderWindowLocked(nowPlaying, cycleReset, linPrev, linCur);
        loopCursorValid = false;
        double pub = nowPlaying ? linCur : linPrev;
        if (haveRegion && pub >= le)
            pub = ls + std::fmod(pub - ls, (double)le - ls);
        loopedBar.store((float)pub, std::memory_order_relaxed);
        snapMutex.unlock();
        return true;
    }

    // Sync the musical cursor to the linear clock on the first looped cycle and
    // after any jump (seek / host relocate). fmod folds a position that already
    // ran past the loop end back into the region.
    if (!loopCursorValid || jumped) {
        double p = linPrev;
        if (p >= le) p = ls + std::fmod(p - ls, (double)le - ls);
        musicalPos      = p;
        loopCursorValid = true;
    }

    // Musical bars to advance this cycle, evaluated at the *musical* position so a
    // tempo change inside the loop region is honoured (rather than reusing the
    // linear delta, which is sampled at a different point on the tempo map).
    const double remainSecs = dtSecs - baseSegOff;
    const double s0         = snapBarToSeconds(musicalPos - curBarOffset);
    double remaining        = snapSecondsToBar(s0 + remainSecs) - snapSecondsToBar(s0);
    if (remaining < 0.0) remaining = 0.0;

    double segOff   = baseSegOff;   // seconds from the cycle's first frame to segStart
    double segStart = musicalPos;
    Reset  segReset = cycleReset;

    for (int guard = 0; guard < 128 && remaining > 1.0e-9; ++guard) {
        const double barsToEnd = (double)le - segStart;
        const double take      = barsToEnd < remaining ? barsToEnd : remaining;
        const double segEnd    = segStart + take;

        segCycleStartSecs = segOff;
        segMusicalStart   = segStart;
        renderWindowLocked(true, segReset, segStart, segEnd);

        remaining -= take;
        segOff += snapBarToSeconds(segEnd   - curBarOffset)
                - snapBarToSeconds(segStart - curBarOffset);

        if ((double)le - segEnd <= 1.0e-4) {
            // Reached the loop end: wrap. The next renderWindowLocked() gets a soft
            // reset, which releases any note still held across the seam (a note-off
            // one frame before the seam) before the post-seam window's note-ons. The
            // seam is a musical move, not a transport one, so controller state — CC
            // automation, pitch bend — carries across it untouched.
            segStart = ls;
            segReset = Reset::Soft;
            if (remaining <= 1.0e-9) {
                // Cycle ends exactly on the seam: still flush held notes now with a
                // zero-width window, so nothing rings into the next cycle.
                segCycleStartSecs = segOff;
                segMusicalStart   = ls;
                renderWindowLocked(true, Reset::Soft, ls, ls);
            }
        } else {
            segStart = segEnd;
            segReset = Reset::None;
        }
    }

    musicalPos = segStart;
    loopedBar.store((float)musicalPos, std::memory_order_relaxed);
    snapMutex.unlock();
    return true;
}

bool Sequencer::renderWindowLocked(bool nowPlaying, Reset reset,
                                   double prevBars, double curBars)
{
    // Release notes still sounding, at the window start (prevBars -> frame 0). A hard
    // reset additionally resets controllers on every instrument channel; a soft one
    // must not, since the transport itself has not moved.
    if (reset != Reset::None) {
        for (auto& an : activeNotes) {
            uint8_t msg[3] = {
                static_cast<uint8_t>(0x80 | (an.channel & 0x0F)),
                static_cast<uint8_t>(an.midiPitch), 0
            };
            emit(an.portName, prevBars, msg, 3);
        }
        activeNotes.clear();
    }

    if (reset == Reset::Hard) {
        // Dedup (port, channel) without a std::set: instance counts are tiny, so a
        // linear scan over the reused resetScratch vector allocates nothing.
        resetScratch.clear();
        for (const auto& track : snap.tracks)
            for (const auto& inst : track.instances) {
                if (inst.portName.empty()) continue;
                bool dup = false;
                for (const auto& [p, c] : resetScratch)
                    if (*p == inst.portName && c == inst.midiChannel) { dup = true; break; }
                if (!dup) resetScratch.push_back({&inst.portName, inst.midiChannel});
            }
        for (const auto& [port, ch] : resetScratch) {
            uint8_t base = static_cast<uint8_t>(0xB0 | (ch & 0x0F));
            uint8_t resetMsg[3]  = { base, 121, 0 };  // Reset All Controllers
            uint8_t allOffMsg[3] = { base, 123, 0 };  // All Notes Off
            emit(*port, prevBars, resetMsg,  3);
            emit(*port, prevBars, allOffMsg, 3);
        }
    }

    if (nowPlaying) {
        // Fire note-offs for notes ending in this window.
        for (auto it = activeNotes.begin(); it != activeNotes.end(); ) {
            if (it->offBar <= curBars) {
                uint8_t msg[3] = {
                    static_cast<uint8_t>(0x80 | (it->channel & 0x0F)),
                    static_cast<uint8_t>(it->midiPitch), 0
                };
                emit(it->portName, it->offBar, msg, 3);
                it = activeNotes.erase(it);
            } else {
                ++it;
            }
        }

        fireNoteEvents (prevBars, curBars);
        fireParamEvents(prevBars, curBars);
    }

    return true;
}

// ── Note event generation (RT thread, snapMutex held) ─────────────────────────

void Sequencer::fireNoteEvents(double prevBars, double curBars)
{
    for (const TrackSnap& track : snap.tracks) {
        for (const InstanceSnap& inst : track.instances) {
            if (inst.patternBeats <= 0.0f) continue;
            if (inst.notes.empty())        continue;
            // Loop instances play forever; the anchor bar is only a phase reference.
            // Finite (song) instances are clamped to their placement.
            if (!inst.loop) {
                if (inst.startBar + inst.length <= prevBars) continue;
                if (inst.startBar >= curBars)                continue;
            }

            double windowStart = inst.loop ? prevBars : std::max(prevBars, (double)inst.startBar);
            double windowEnd   = inst.loop ? curBars  : std::min(curBars,  (double)inst.startBar + inst.length);
            if (windowEnd <= windowStart) continue;
            double beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            double beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const NoteSnap& note : inst.notes) {
                forEachFiring(note.beat, inst.patternBeats, beatStart, beatEnd,
                              [&](double firstFire) {
                    double onBar = inst.startBar
                                 + (firstFire - inst.startOffset) / inst.beatsPerBar;
                    uint8_t vel = static_cast<uint8_t>(
                        std::clamp(static_cast<int>(note.velocity * 127), 1, 127));
                    uint8_t onMsg[3] = {
                        static_cast<uint8_t>(0x90 | (inst.midiChannel & 0x0F)),
                        static_cast<uint8_t>(note.midiPitch), vel
                    };
                    emit(inst.portName, onBar, onMsg, 3);

                    double offBar = inst.startBar
                                  + (firstFire + note.length - inst.startOffset) / inst.beatsPerBar;
                    ActiveNote an{note.midiPitch, inst.midiChannel, offBar, {}};
                    std::strncpy(an.portName, inst.portName.c_str(), sizeof(an.portName) - 1);
                    activeNotes.push_back(an);
                });
            }
        }
    }
}

// ── Param event generation (RT thread, snapMutex held) ────────────────────────

void Sequencer::fireParamEvents(double prevBars, double curBars)
{
    paramScratch.clear();

    for (const ParamInstSnap& inst : snap.paramInsts) {
        if (inst.events.empty()) continue;

        if (inst.patternBeats == 0.0f) {
            // Song-level: event.beat is a bar position; no looping.
            for (const ParamEventSnap& evt : inst.events) {
                if (evt.beat < prevBars || evt.beat >= curBars) continue;
                paramScratch.push_back({evt.beat, &inst.portName, inst.midiChannel,
                                        inst.ccNumber, evt.value, inst.priority});
            }
        } else {
            // Pattern-level: event.beat is a within-pattern beat; wraps with patternBeats.
            if (!inst.loop) {
                if (inst.startBar + inst.length <= prevBars) continue;
                if (inst.startBar >= curBars)                continue;
            }

            double windowStart = inst.loop ? prevBars : std::max(prevBars, (double)inst.startBar);
            double windowEnd   = inst.loop ? curBars  : std::min(curBars,  (double)inst.startBar + inst.length);
            if (windowEnd <= windowStart) continue;
            double beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            double beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const ParamEventSnap& evt : inst.events) {
                forEachFiring(evt.beat, inst.patternBeats, beatStart, beatEnd,
                              [&](double firstFire) {
                    double onBar = inst.startBar + (firstFire - inst.startOffset) / inst.beatsPerBar;
                    paramScratch.push_back({onBar, &inst.portName, inst.midiChannel,
                                            inst.ccNumber, evt.value, inst.priority});
                });
            }
        }
    }

    if (paramScratch.empty()) return;

    // Sort by (portName, channel, cc, bar) then priority descending so the
    // highest-priority entry for each combination comes first.
    std::sort(paramScratch.begin(), paramScratch.end(), [](const PendingParam& a, const PendingParam& b) {
        if (a.portName    != b.portName)    return *a.portName    < *b.portName;
        if (a.midiChannel != b.midiChannel) return a.midiChannel  < b.midiChannel;
        if (a.ccNumber    != b.ccNumber)    return a.ccNumber      < b.ccNumber;
        if (a.bar         != b.bar)         return a.bar           < b.bar;
        return a.priority > b.priority;   // higher priority first within same slot
    });

    for (int i = 0; i < (int)paramScratch.size(); ) {
        const PendingParam& p = paramScratch[i];
        // Advance past lower-priority duplicates for the same (port,ch,cc,bar).
        int j = i + 1;
        while (j < (int)paramScratch.size()              &&
               *paramScratch[j].portName == *p.portName  &&
               paramScratch[j].midiChannel == p.midiChannel &&
               paramScratch[j].ccNumber    == p.ccNumber    &&
               paramScratch[j].bar         == p.bar)
            ++j;

        if (p.ccNumber < 0) {
            // Pitch bend: value is already 0-16383 (14-bit).
            int     val14 = p.value;
            uint8_t msg[3] = {
                static_cast<uint8_t>(0xE0 | (p.midiChannel & 0x0F)),
                static_cast<uint8_t>(val14 & 0x7F),
                static_cast<uint8_t>((val14 >> 7) & 0x7F)
            };
            emit(*p.portName, p.bar, msg, 3);
        } else {
            uint8_t msg[3] = {
                static_cast<uint8_t>(0xB0 | (p.midiChannel & 0x0F)),
                static_cast<uint8_t>(p.ccNumber & 0x7F),
                static_cast<uint8_t>(p.value    & 0x7F)
            };
            emit(*p.portName, p.bar, msg, 3);
        }
        i = j;
    }
}
