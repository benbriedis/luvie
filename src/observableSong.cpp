#include "observableSong.hpp"
#include "paramLaneTypes.hpp"
#include <algorithm>
#include <set>
#include <cmath>

ObservableSong::ObservableSong(float initBpm, int initTop, int initBottom)
{
    data.bpms.push_back({0, initBpm});
    data.timeSigs.push_back({0, initTop, initBottom});
}

void ObservableSong::addObserver(ITimelineObserver* o)
{
    observers.push_back(o);
}

void ObservableSong::removeObserver(ITimelineObserver* o)
{
    observers.erase(std::remove(observers.begin(), observers.end(), o), observers.end());
}

void ObservableSong::notify()
{
    auto copy = observers;
    for (auto* o : copy) o->onTimelineChanged();
}

void ObservableSong::loadTimeline(const Timeline& tl)
{
    data = tl;
    if (data.rowOrder.empty()) {
        for (const auto& t : data.tracks)     data.rowOrder.push_back({true,  t.id});
        for (const auto& l : data.paramLanes) data.rowOrder.push_back({false, l.id});
    }
    nextId = 1;
    for (const auto& p : data.patterns) {
        if (p.id >= nextId) nextId = p.id + 1;
        for (const auto& n : p.notes)
            if (n.id >= nextId) nextId = n.id + 1;
        for (const auto& dn : p.drumNotes)
            if (dn.id >= nextId) nextId = dn.id + 1;
    }
    for (const auto& t : data.tracks) {
        if (t.id >= nextId) nextId = t.id + 1;
        for (const auto& inst : t.patterns)
            if (inst.id >= nextId) nextId = inst.id + 1;
    }
    for (const auto& lane : data.paramLanes) {
        if (lane.id >= nextId) nextId = lane.id + 1;
        for (const auto& pt : lane.points)
            if (pt.id >= nextId) nextId = pt.id + 1;
    }
    notify();
}

void ObservableSong::sortBpms()
{
    std::sort(data.bpms.begin(), data.bpms.end(),
        [](const BpmMarker& a, const BpmMarker& b) { return a.bar < b.bar; });
}

void ObservableSong::sortTimeSigs()
{
    std::sort(data.timeSigs.begin(), data.timeSigs.end(),
        [](const TimeSigMarker& a, const TimeSigMarker& b) { return a.bar < b.bar; });
}

void ObservableSong::setBpm(int bar, float bpm)
{
    for (auto& m : data.bpms) {
        if (m.bar == bar) { m.bpm = bpm; notify(); return; }
    }
    data.bpms.push_back({bar, bpm});
    sortBpms();
    notify();
}

void ObservableSong::removeBpm(int bar)
{
    if (bar == 0) return;
    data.bpms.erase(std::remove_if(data.bpms.begin(), data.bpms.end(),
        [bar](const BpmMarker& m) { return m.bar == bar; }), data.bpms.end());
    notify();
}

void ObservableSong::moveBpmMarker(int fromBar, int toBar)
{
    if (fromBar == 0) return;
    auto it = std::find_if(data.bpms.begin(), data.bpms.end(),
        [fromBar](const BpmMarker& m) { return m.bar == fromBar; });
    if (it == data.bpms.end()) return;
    float val = it->bpm;
    data.bpms.erase(it);
    data.bpms.erase(std::remove_if(data.bpms.begin(), data.bpms.end(),
        [toBar](const BpmMarker& m) { return m.bar == toBar; }), data.bpms.end());
    data.bpms.push_back({toBar, val});
    sortBpms();
    notify();
}

float ObservableSong::bpmAt(int bar) const
{
    float result = 120.0f;
    for (auto& m : data.bpms) {
        if (m.bar <= bar) result = m.bpm;
        else break;
    }
    return result;
}

void ObservableSong::setTimeSig(int bar, int top, int bottom)
{
    for (auto& m : data.timeSigs) {
        if (m.bar == bar) { m.top = top; m.bottom = bottom; notify(); return; }
    }
    data.timeSigs.push_back({bar, top, bottom});
    sortTimeSigs();
    notify();
}

void ObservableSong::removeTimeSig(int bar)
{
    if (bar == 0) return;
    data.timeSigs.erase(std::remove_if(data.timeSigs.begin(), data.timeSigs.end(),
        [bar](const TimeSigMarker& m) { return m.bar == bar; }), data.timeSigs.end());
    notify();
}

void ObservableSong::moveTimeSigMarker(int fromBar, int toBar)
{
    if (fromBar == 0) return;
    auto it = std::find_if(data.timeSigs.begin(), data.timeSigs.end(),
        [fromBar](const TimeSigMarker& m) { return m.bar == fromBar; });
    if (it == data.timeSigs.end()) return;
    int top = it->top, bottom = it->bottom;
    data.timeSigs.erase(it);
    data.timeSigs.erase(std::remove_if(data.timeSigs.begin(), data.timeSigs.end(),
        [toBar](const TimeSigMarker& m) { return m.bar == toBar; }), data.timeSigs.end());
    data.timeSigs.push_back({toBar, top, bottom});
    sortTimeSigs();
    notify();
}

void ObservableSong::timeSigAt(int bar, int& top, int& bottom) const
{
    top = 4; bottom = 4;
    for (auto& m : data.timeSigs) {
        if (m.bar <= bar) { top = m.top; bottom = m.bottom; }
        else break;
    }
}

// ---------------------------------------------------------------------------

std::vector<ObservableSong::TimeSegment> ObservableSong::buildSegments() const
{
    std::set<int> breakpoints;
    for (auto& m : data.bpms)    breakpoints.insert(m.bar);
    for (auto& m : data.timeSigs) breakpoints.insert(m.bar);

    std::vector<TimeSegment> segs;
    for (int bar : breakpoints) {
        int top, bottom;
        timeSigAt(bar, top, bottom);
        segs.push_back({bar, bpmAt(bar), top});
    }
    return segs;
}

double ObservableSong::barToSeconds(float targetBar) const
{
    auto segs = buildSegments();
    double secs = 0.0;
    for (int i = 0; i < (int)segs.size(); i++) {
        float segStart = (float)segs[i].bar;
        float segEnd   = (i + 1 < (int)segs.size()) ? (float)segs[i+1].bar : targetBar;
        if (segStart >= targetBar) break;
        float barsInSeg = std::min(segEnd, targetBar) - segStart;
        double secsPerBar = segs[i].beatsPerBar * 60.0 / segs[i].bpm;
        secs += barsInSeg * secsPerBar;
    }
    return secs;
}

float ObservableSong::secondsToBar(double secs) const
{
    auto segs = buildSegments();
    double remaining = secs;
    for (int i = 0; i < (int)segs.size(); i++) {
        float  segStart   = (float)segs[i].bar;
        float  segEnd     = (i + 1 < (int)segs.size()) ? (float)segs[i+1].bar : 1e9f;
        double secsPerBar = segs[i].beatsPerBar * 60.0 / segs[i].bpm;
        double segSecs    = (segEnd - segStart) * secsPerBar;
        if (remaining < segSecs + 1e-9)
            return segStart + (float)(remaining / secsPerBar);
        remaining -= segSecs;
    }
    if (!segs.empty()) {
        auto& last = segs.back();
        double secsPerBar = last.beatsPerBar * 60.0 / last.bpm;
        return (float)(last.bar + remaining / secsPerBar);
    }
    return 0.0f;
}

void ObservableSong::secondsToBarBeat(double secs, int& bar, int& beat) const
{
    float barF   = secondsToBar(secs);
    int   barInt = (int)barF;
    int   top, bottom;
    timeSigAt(barInt, top, bottom);
    bar  = barInt + 1;
    beat = (int)((barF - (float)barInt) * top) + 1;
}

// ---------------------------------------------------------------------------
// Track management

int ObservableSong::addTrack(std::string label, int patternId, int atIndex)
{
    int id = nextId++;
    data.tracks.push_back({id, std::move(label), patternId, {}});
    RowRef ref{true, id};
    if (atIndex >= 0 && atIndex <= (int)data.rowOrder.size())
        data.rowOrder.insert(data.rowOrder.begin() + atIndex, ref);
    else
        data.rowOrder.push_back(ref);
    notify();
    return id;
}

int ObservableSong::trackIndexForId(int trackId) const
{
    for (int i = 0; i < (int)data.tracks.size(); i++)
        if (data.tracks[i].id == trackId) return i;
    return -1;
}

void ObservableSong::moveRow(int from, int toGap)
{
    auto& ro = data.rowOrder;
    int n = (int)ro.size();
    if (from < 0 || from >= n) return;
    if (toGap < 0 || toGap > n) return;
    if (toGap == from || toGap == from + 1) return;
    RowRef ref = ro[from];
    ro.erase(ro.begin() + from);
    int insertAt = (toGap > from) ? toGap - 1 : toGap;
    ro.insert(ro.begin() + insertAt, ref);
    notify();
}

void ObservableSong::renameTrack(int trackId, std::string newLabel)
{
    for (auto& t : data.tracks)
        if (t.id == trackId) { t.label = std::move(newLabel); notify(); return; }
}

void ObservableSong::selectTrack(int index)
{
    data.selectedTrackIndex = index;
    notify();
}

void ObservableSong::removeTrack(int trackId)
{
    data.tracks.erase(
        std::remove_if(data.tracks.begin(), data.tracks.end(),
            [trackId](const Track& t) { return t.id == trackId; }),
        data.tracks.end());
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [trackId](const RowRef& r) { return r.isTrack && r.id == trackId; }),
        data.rowOrder.end());
    notify();
}

int ObservableSong::copyPattern(int srcPatId)
{
    const Pattern* src = nullptr;
    for (const auto& p : data.patterns)
        if (p.id == srcPatId) { src = &p; break; }
    if (!src) return -1;

    Pattern copy;
    copy.id = nextId++;
    copy.lengthBeats = src->lengthBeats;
    copy.type = src->type;
    for (auto n : src->notes) {
        n.id = nextId++;
        copy.notes.push_back(n);
    }
    for (auto n : src->drumNotes) {
        n.id = nextId++;
        copy.drumNotes.push_back(n);
    }
    data.patterns.push_back(copy);
    return copy.id;
}

int ObservableSong::nextTrackNumberForType(PatternType type) const
{
    int count = 0;
    for (const auto& t : data.tracks) {
        for (const auto& p : data.patterns) {
            if (p.id == t.patternId && p.type == type) { ++count; break; }
        }
    }
    return count + 1;
}

void ObservableSong::removeTrackAndPattern(int trackId)
{
    auto it = std::find_if(data.tracks.begin(), data.tracks.end(),
        [trackId](const Track& t) { return t.id == trackId; });
    if (it == data.tracks.end()) return;

    int patId = it->patternId;
    data.tracks.erase(it);
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [trackId](const RowRef& r) { return r.isTrack && r.id == trackId; }),
        data.rowOrder.end());

    bool stillUsed = std::any_of(data.tracks.begin(), data.tracks.end(),
        [patId](const Track& t) { return t.patternId == patId; });
    if (!stillUsed && patId > 0)
        data.patterns.erase(std::remove_if(data.patterns.begin(), data.patterns.end(),
            [patId](const Pattern& p) { return p.id == patId; }), data.patterns.end());

    if (data.selectedTrackIndex >= (int)data.tracks.size())
        data.selectedTrackIndex = std::max(0, (int)data.tracks.size() - 1);
    notify();
}

// ---------------------------------------------------------------------------
// Pattern lifecycle

int ObservableSong::createPattern(float lengthBeats)
{
    int id = nextId++;
    Pattern p;
    p.id = id;
    p.lengthBeats = lengthBeats;
    p.outputInstrumentName = defaultOutputInstrument;
    timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    data.patterns.push_back(p);
    notify();
    return id;
}

int ObservableSong::createDrumPattern(float lengthBeats)
{
    int id = nextId++;
    Pattern p;
    p.id = id;
    p.lengthBeats = lengthBeats;
    p.type = PatternType::DRUM;
    p.outputInstrumentName = defaultDrumOutputInstrument.empty()
        ? defaultOutputInstrument : defaultDrumOutputInstrument;
    timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    data.patterns.push_back(std::move(p));
    notify();
    return id;
}

int ObservableSong::createPianorollPattern(float lengthBeats)
{
    int id = nextId++;
    Pattern p;
    p.id = id;
    p.lengthBeats = lengthBeats;
    p.type = PatternType::PIANOROLL;
    p.outputInstrumentName = defaultOutputInstrument;
    timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    data.patterns.push_back(std::move(p));
    notify();
    return id;
}

const PatternInstance* ObservableSong::instanceById(int instanceId) const
{
    for (const auto& track : data.tracks)
        for (const auto& inst : track.patterns)
            if (inst.id == instanceId)
                return &inst;
    return nullptr;
}

const Pattern* ObservableSong::patternForInstance(int instanceId) const
{
    for (const auto& track : data.tracks)
        for (const auto& inst : track.patterns)
            if (inst.id == instanceId) {
                for (const auto& pat : data.patterns)
                    if (pat.id == inst.patternId)
                        return &pat;
                return nullptr;
            }
    return nullptr;
}

void ObservableSong::renamePatternOutputInstrument(const std::string& oldName, const std::string& newName)
{
    bool changed = false;
    for (auto& p : data.patterns) {
        if (p.outputInstrumentName == oldName) {
            p.outputInstrumentName = newName;
            changed = true;
        }
    }
    if (changed) notify();
}

// ---------------------------------------------------------------------------
// Pattern instance management

void ObservableSong::addPattern(int trackIndex, float startBar, float length, float patternBeats)
{
    if (trackIndex < 0 || trackIndex >= (int)data.tracks.size()) return;
    int patId = 0;
    if (patternBeats > 0.0f) {
        patId = nextId++;
        data.patterns.push_back({patId, patternBeats});
    }
    data.tracks[trackIndex].patterns.push_back({nextId++, patId, startBar, length});
    notify();
}

void ObservableSong::removePattern(int instanceId)
{
    for (auto& track : data.tracks) {
        auto it = std::find_if(track.patterns.begin(), track.patterns.end(),
            [instanceId](const PatternInstance& p) { return p.id == instanceId; });
        if (it != track.patterns.end()) {
            int patId = it->patternId;
            track.patterns.erase(it);
            if (patId > 0) {
                bool stillUsed = false;
                for (const auto& t : data.tracks)
                    for (const auto& p : t.patterns)
                        if (p.patternId == patId) { stillUsed = true; break; }
                if (!stillUsed)
                    data.patterns.erase(std::remove_if(data.patterns.begin(), data.patterns.end(),
                        [patId](const Pattern& p) { return p.id == patId; }), data.patterns.end());
            }
            notify();
            return;
        }
    }
}

void ObservableSong::movePattern(int instanceId, int newTrackIndex, float newStartBar)
{
    if (newTrackIndex < 0 || newTrackIndex >= (int)data.tracks.size()) return;
    for (auto& track : data.tracks) {
        auto it = std::find_if(track.patterns.begin(), track.patterns.end(),
            [instanceId](const PatternInstance& p) { return p.id == instanceId; });
        if (it != track.patterns.end()) {
            PatternInstance inst = *it;
            track.patterns.erase(it);
            inst.startBar = newStartBar;
            data.tracks[newTrackIndex].patterns.push_back(std::move(inst));
            notify();
            return;
        }
    }
}

void ObservableSong::resizePattern(int patternId, float newLength)
{
    for (auto& track : data.tracks) {
        for (auto& p : track.patterns) {
            if (p.id == patternId) {
                p.length = newLength;
                notify();
                return;
            }
        }
    }
}

void ObservableSong::resizePatternLeft(int patternId, float newStartBar, float newLength, float newStartOffset)
{
    for (auto& track : data.tracks)
        for (auto& p : track.patterns)
            if (p.id == patternId) {
                p.startBar    = newStartBar;
                p.length      = newLength;
                p.startOffset = newStartOffset;
                notify();
                return;
            }
}

void ObservableSong::setPatternStartOffset(int patternId, float startOffset)
{
    for (auto& track : data.tracks)
        for (auto& p : track.patterns)
            if (p.id == patternId) { p.startOffset = startOffset; notify(); return; }
}

void ObservableSong::placePattern(int trackIndex, int patternId, float startBar, float length)
{
    if (trackIndex < 0 || trackIndex >= (int)data.tracks.size()) return;
    data.tracks[trackIndex].patterns.push_back({nextId++, patternId, startBar, length});
    notify();
}

std::vector<Note> ObservableSong::buildNotes() const
{
    std::vector<Note> notes;
    for (int row = 0; row < (int)data.rowOrder.size(); row++) {
        const auto& ref = data.rowOrder[row];
        if (!ref.isTrack) continue;
        for (const auto& t : data.tracks)
            if (t.id == ref.id) {
                for (const auto& p : t.patterns)
                    notes.push_back({p.id, row, p.startBar, p.length});
                break;
            }
    }
    return notes;
}

bool ObservableSong::hasParamLane(const std::string& type) const
{
    for (const auto& lane : data.paramLanes)
        if (lane.type == type) return true;
    return false;
}

int ObservableSong::addParamLane(const std::string& type, int atIndex)
{
    int laneId = nextId++;
    ParamLane lane;
    lane.id   = laneId;
    lane.type = type;
    lane.points.push_back({nextId++, 0.0f, laneDefaultValue(type), true});
    data.paramLanes.push_back(std::move(lane));
    RowRef ref{false, laneId};
    if (atIndex >= 0 && atIndex <= (int)data.rowOrder.size())
        data.rowOrder.insert(data.rowOrder.begin() + atIndex, ref);
    else
        data.rowOrder.push_back(ref);
    notify();
    return laneId;
}

void ObservableSong::removeParamLane(int laneId)
{
    data.paramLanes.erase(
        std::remove_if(data.paramLanes.begin(), data.paramLanes.end(),
            [laneId](const ParamLane& l) { return l.id == laneId; }),
        data.paramLanes.end());
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [laneId](const RowRef& r) { return !r.isTrack && r.id == laneId; }),
        data.rowOrder.end());
    notify();
}

int ObservableSong::addParamPoint(int laneId, float beat, int value)
{
    for (auto& lane : data.paramLanes) {
        if (lane.id != laneId) continue;
        int ptId = nextId++;
        auto it = std::lower_bound(lane.points.begin(), lane.points.end(),
            beat, [](const ParamPoint& p, float b) { return p.beat < b; });
        lane.points.insert(it, {ptId, beat, std::clamp(value, 0, laneMaxValue(lane.type)), false});
        notify();
        return ptId;
    }
    return -1;
}

void ObservableSong::removeParamPoint(int pointId)
{
    for (auto& lane : data.paramLanes) {
        auto it = std::find_if(lane.points.begin(), lane.points.end(),
            [pointId](const ParamPoint& p) { return p.id == pointId; });
        if (it != lane.points.end() && !it->anchor) {
            lane.points.erase(it);
            notify();
            return;
        }
    }
}

void ObservableSong::moveParamPoint(int pointId, float beat, int value)
{
    for (auto& lane : data.paramLanes) {
        for (auto& pt : lane.points) {
            if (pt.id != pointId) continue;
            if (!pt.anchor)
                pt.beat  = std::max(0.0f, beat);
            pt.value = std::clamp(value, 0, laneMaxValue(lane.type));
            std::stable_sort(lane.points.begin(), lane.points.end(),
                [](const ParamPoint& a, const ParamPoint& b) { return a.beat < b.beat; });
            notify();
            return;
        }
    }
}
