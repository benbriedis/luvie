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
    // Pre-reserve every RT-thread scratch buffer so renderWindow() never allocates,
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
    loopMode = mode;
    rebuildSnapshot();
}

// ── Snapshot building (owner thread) ──────────────────────────────────────────

void Sequencer::rebuildSnapshot()
{
    if (!timeline) return;

    const Timeline& tl = timeline->get();
    Snapshot newSnap;

    // Build time segments at each tempo / time-signature boundary.
    {
        std::set<int> breakpoints;
        breakpoints.insert(0);
        for (auto& m : tl.bpms)     breakpoints.insert(m.bar);
        for (auto& m : tl.timeSigs) breakpoints.insert(m.bar);

        double accSecs      = 0.0;
        int    prevBar      = 0;
        double prevSecsPerBar = timeline->secondsPerBarAt(0);

        for (int bar : breakpoints) {
            if (bar != 0)
                accSecs += (bar - prevBar) * prevSecsPerBar;

            int top, bot;
            timeline->timeSigAt(bar, top, bot);
            double barCrotchets = timeSettings::barCrotchets(top, bot);
            float  cpm          = timeline->cpmAt(bar);

            newSnap.segs.push_back({(float)bar, cpm, top, barCrotchets, accSecs});

            prevBar        = bar;
            prevSecsPerBar = timeSettings::secondsPerBar(barCrotchets, cpm);
        }
    }

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
            for (const Note& note : pat->notes) {
                if (note.disabled) continue;
                int midi = std::clamp(rowToMidi(note.row, pat->rootPitch, chordIndex) + pat->octaveOffset * 12, 0, 127);
                is.notes.push_back({midi, note.beat, note.length, note.velocity});
            }
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
        int top, bot;
        timeline->timeSigAt((int)std::max(0.0f, anchorBar), top, bot);
        is.beatsPerBar = (float)top;
        buildNotes(is, pat, trackIdx, trackInstrument);

        // Param lanes. Build BEFORE moving `is` below — moving leaves portName empty.
        for (const auto& lane : pat->paramLanes) {
            auto evts = buildParamEvents(lane);
            if (evts.empty()) continue;
            ParamInstSnap pis;
            pis.startBar     = anchorBar;
            pis.length       = 1.0e9f;
            pis.startOffset  = 0.0f;
            pis.beatsPerBar  = (float)top;
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
        if (!loopMgr) return;
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
                int top, bot;
                timeline->timeSigAt((int)inst.startBar, top, bot);
                is.beatsPerBar = (float)top;
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
                        pis.beatsPerBar  = (float)top;
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

    {
        std::lock_guard<std::mutex> lock(snapMutex);
        snap = std::move(newSnap);
    }
}

// ── RT-safe bar/seconds conversion ────────────────────────────────────────────

double Sequencer::snapBarToSeconds(float bar) const
{
    for (int i = 0; i + 1 < (int)snap.segs.size(); i++) {
        if (bar < snap.segs[i + 1].bar) {
            double secsPerBar = timeSettings::secondsPerBar(snap.segs[i].barCrotchets, snap.segs[i].cpm);
            return snap.segs[i].startSecs + (bar - snap.segs[i].bar) * secsPerBar;
        }
    }
    if (!snap.segs.empty()) {
        auto& last = snap.segs.back();
        double secsPerBar = timeSettings::secondsPerBar(last.barCrotchets, last.cpm);
        return last.startSecs + (bar - last.bar) * secsPerBar;
    }
    return 0.0;
}

float Sequencer::snapSecondsToBar(double secs) const
{
    for (int i = 0; i + 1 < (int)snap.segs.size(); i++) {
        double segStart = snap.segs[i].startSecs;
        double segEnd   = snap.segs[i + 1].startSecs;
        if (secs < segEnd) {
            double secsPerBar = timeSettings::secondsPerBar(snap.segs[i].barCrotchets, snap.segs[i].cpm);
            return snap.segs[i].bar + (float)((secs - segStart) / secsPerBar);
        }
    }
    if (!snap.segs.empty()) {
        auto& last = snap.segs.back();
        double secsPerBar = timeSettings::secondsPerBar(last.barCrotchets, last.cpm);
        return last.bar + (float)((secs - last.startSecs) / secsPerBar);
    }
    return 0.0f;
}

// ── Cycle rendering (RT thread) ───────────────────────────────────────────────

bool Sequencer::renderWindow(bool nowPlaying, bool jumped,
                             float prevBars, float curBars)
{
    // Hold the snapshot for the whole cycle: emit() reads snap.segs (via
    // snapBarToSeconds) for every message, so the lock must cover note-offs too.
    // try_lock keeps the RT thread from blocking on the owner thread's rebuild; on
    // failure we bail and the caller leaves wasPlaying untouched so the missed
    // stop/jump is retried next cycle.
    if (!snapMutex.try_lock())
        return false;

    // On stop or jump: silence active notes, then reset all controllers on
    // instrument channels. Emitted at the cycle start (prevBars -> frame 0).
    if ((!nowPlaying && wasPlaying) || jumped) {
        for (auto& an : activeNotes) {
            uint8_t msg[3] = {
                static_cast<uint8_t>(0x80 | (an.channel & 0x0F)),
                static_cast<uint8_t>(an.midiPitch), 0
            };
            emit(an.portName, prevBars, msg, 3);
        }
        activeNotes.clear();

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

    snapMutex.unlock();
    return true;
}

// ── Note event generation (RT thread, snapMutex held) ─────────────────────────

void Sequencer::fireNoteEvents(float prevBars, float curBars)
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

            float windowStart = inst.loop ? prevBars : std::max(prevBars, inst.startBar);
            float windowEnd   = inst.loop ? curBars  : std::min(curBars,  inst.startBar + inst.length);
            if (windowEnd <= windowStart) continue;
            float beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            float beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const NoteSnap& note : inst.notes) {
                forEachFiring(note.beat, inst.patternBeats, beatStart, beatEnd,
                              [&](float firstFire) {
                    float onBar = inst.startBar
                                + (firstFire - inst.startOffset) / inst.beatsPerBar;
                    uint8_t vel = static_cast<uint8_t>(
                        std::clamp(static_cast<int>(note.velocity * 127), 1, 127));
                    uint8_t onMsg[3] = {
                        static_cast<uint8_t>(0x90 | (inst.midiChannel & 0x0F)),
                        static_cast<uint8_t>(note.midiPitch), vel
                    };
                    emit(inst.portName, onBar, onMsg, 3);

                    float offBar = inst.startBar
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

void Sequencer::fireParamEvents(float prevBars, float curBars)
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

            float windowStart = inst.loop ? prevBars : std::max(prevBars, inst.startBar);
            float windowEnd   = inst.loop ? curBars  : std::min(curBars,  inst.startBar + inst.length);
            if (windowEnd <= windowStart) continue;
            float beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            float beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const ParamEventSnap& evt : inst.events) {
                forEachFiring(evt.beat, inst.patternBeats, beatStart, beatEnd,
                              [&](float firstFire) {
                    float onBar = inst.startBar + (firstFire - inst.startOffset) / inst.beatsPerBar;
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
