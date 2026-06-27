#include "observableSong.hpp"
#include "paramLaneTypes.hpp"
#include <algorithm>
#include <set>
#include <unordered_map>
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
    // Build rowOrder if absent, or migrate old files where track rows used Track IDs.
    {
        std::set<int> laneIds;
        for (const auto& t : data.tracks)
            for (const auto& l : t.lanes) laneIds.insert(l.id);
        bool needsRebuild = data.rowOrder.empty();
        if (!needsRebuild)
            for (const auto& r : data.rowOrder)
                if (r.kind == RowKind::Lane && !laneIds.count(r.id)) { needsRebuild = true; break; }
        if (needsRebuild) {
            data.rowOrder.clear();
            for (const auto& t : data.tracks)
                for (const auto& l : t.lanes)
                    data.rowOrder.push_back({RowKind::Lane, l.id});
            for (const auto& l : data.paramLanes) data.rowOrder.push_back({RowKind::Param, l.id});
        }
    }
    nextId = 1;
    nextPatternNumber = (int)data.patterns.size() + 1;
    for (const auto& instr : data.instruments)
        if (instr.id >= nextId) nextId = instr.id + 1;
    for (const auto& p : data.patterns) {
        if (p.id >= nextId) nextId = p.id + 1;
        for (const auto& n : p.notes)
            if (n.id >= nextId) nextId = n.id + 1;
        for (const auto& dn : p.drumNotes)
            if (dn.id >= nextId) nextId = dn.id + 1;
    }
    for (auto& t : data.tracks) {
        if (t.id >= nextId) nextId = t.id + 1;
        for (auto& lane : t.lanes) {
            if (lane.id == 0) lane.id = nextId++;  // assign ID to lanes from old saves
            else if (lane.id >= nextId) nextId = lane.id + 1;
            for (const auto& inst : lane.patterns)
                if (inst.id >= nextId) nextId = inst.id + 1;
        }
    }
    for (const auto& lane : data.paramLanes) {
        if (lane.id >= nextId) nextId = lane.id + 1;
        for (const auto& pt : lane.points)
            if (pt.id >= nextId) nextId = pt.id + 1;
    }
    // Initialise selectedLaneId if missing (old save files won't have it).
    if (data.selectedLaneId == -1) {
        int idx = data.selectedTrackIndex;
        if (idx >= 0 && idx < (int)data.tracks.size() && !data.tracks[idx].lanes.empty())
            data.selectedLaneId = data.tracks[idx].lanes[0].id;
    }
    rebuildInstrumentHeaders();
    notify();
}

void ObservableSong::rebuildInstrumentHeaders()
{
    // Reposition headers for non-empty unstacked tracks (anchored before their first lane).
    // Remove headers for non-empty stacked tracks (single combined row, no separate header).
    // Headers for empty tracks stay where they are — they were placed when the last lane left.
    std::set<int> nonEmptyUnstackedIds;
    std::set<int> nonEmptyStackedIds;
    for (const auto& t : data.tracks) {
        if (!t.lanes.empty()) {
            if (!t.stackedLanes) nonEmptyUnstackedIds.insert(t.id);
            else                 nonEmptyStackedIds.insert(t.id);
        }
    }

    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [&nonEmptyUnstackedIds, &nonEmptyStackedIds](const RowRef& r) {
                return r.kind == RowKind::Header &&
                       (nonEmptyUnstackedIds.count(r.id) || nonEmptyStackedIds.count(r.id));
            }),
        data.rowOrder.end());

    for (const auto& t : data.tracks) {
        if (t.stackedLanes || t.lanes.empty()) continue;
        int firstLaneId = t.lanes[0].id;
        for (int i = 0; i < (int)data.rowOrder.size(); i++) {
            if (data.rowOrder[i].kind == RowKind::Lane && data.rowOrder[i].id == firstLaneId) {
                data.rowOrder.insert(data.rowOrder.begin() + i, RowRef{RowKind::Header, t.id});
                break;
            }
        }
    }
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
// Instrument management

int ObservableSong::addInstrument(std::string name, bool isDrum)
{
    int id = nextId++;
    data.instruments.push_back({id, std::move(name), isDrum});
    notify();
    return id;
}

void ObservableSong::renameInstrument(int instrId, std::string name)
{
    for (auto& i : data.instruments)
        if (i.id == instrId) { i.name = std::move(name); notify(); return; }
}

void ObservableSong::removeInstrument(int instrId)
{
    data.instruments.erase(
        std::remove_if(data.instruments.begin(), data.instruments.end(),
            [instrId](const Instrument& i) { return i.id == instrId; }),
        data.instruments.end());
    notify();
}

// ---------------------------------------------------------------------------
// Track management

int ObservableSong::addTrack(int instrumentId, int patternId, int atIndex)
{
    int id     = nextId++;
    int laneId = nextId++;
    Lane lane{laneId, patternId, {}};
    Track newTrack;
    newTrack.id           = id;
    newTrack.instrumentId = instrumentId;
    newTrack.lanes.push_back(std::move(lane));
    data.tracks.push_back(std::move(newTrack));
    RowRef ref{RowKind::Lane, laneId};
    if (atIndex >= 0 && atIndex <= (int)data.rowOrder.size())
        data.rowOrder.insert(data.rowOrder.begin() + atIndex, ref);
    else
        data.rowOrder.push_back(ref);
    rebuildInstrumentHeaders();
    notify();
    return id;
}

int ObservableSong::trackIndexForId(int trackId) const
{
    for (int i = 0; i < (int)data.tracks.size(); i++)
        if (data.tracks[i].id == trackId) return i;
    return -1;
}

int ObservableSong::trackIndexForLaneId(int laneId) const
{
    for (int i = 0; i < (int)data.tracks.size(); i++)
        for (const auto& l : data.tracks[i].lanes)
            if (l.id == laneId) return i;
    return -1;
}

int ObservableSong::trackIdForLaneId(int laneId) const
{
    for (const auto& t : data.tracks)
        for (const auto& l : t.lanes)
            if (l.id == laneId) return t.id;
    return -1;
}

int ObservableSong::instrumentIdForTrack(int trackId) const
{
    for (const auto& t : data.tracks)
        if (t.id == trackId) return t.instrumentId;
    return 0;
}

ObservableSong::TrackMenuFlags ObservableSong::trackMenuFlags(int trackId) const
{
    TrackMenuFlags flags;
    for (const auto& t : data.tracks) {
        if (t.id != trackId) continue;
        flags.canRemoveLane  = !t.stackedLanes && !t.lanes.empty();
        flags.canOpenPattern = !t.stackedLanes && !t.lanes.empty();
        if (!t.lanes.empty()) {
            int patId = t.lanes[0].patternId;
            for (const auto& p : data.patterns)
                if (p.id == patId) { flags.isDrumTrack = (p.type == PatternType::DRUM); break; }
        }
        break;
    }
    return flags;
}

int ObservableSong::paramLaneInsertIndex(int trackId) const
{
    // Insert after the last visible lane row belonging to this track.
    for (const auto& t : data.tracks) {
        if (t.id != trackId || t.lanes.empty()) continue;
        for (int li = (int)t.lanes.size() - 1; li >= 0; li--) {
            int laneId = t.lanes[li].id;
            for (int i = (int)data.rowOrder.size() - 1; i >= 0; i--)
                if (data.rowOrder[i].kind == RowKind::Lane && data.rowOrder[i].id == laneId)
                    return i + 1;
        }
        break;
    }
    return -1;
}

void ObservableSong::moveRow(int from, int toGap)
{
    auto& ro = data.rowOrder;
    int n = (int)ro.size();
    if (from < 0 || from >= n) return;
    if (ro[from].kind == RowKind::Header) return;  // don't drag header rows directly
    if (toGap < 0 || toGap > n) return;
    if (toGap == from || toGap == from + 1) return;

    RowRef ref = ro[from];

    // Record which track owns this lane before we touch rowOrder
    int srcTrackId = -1;
    if (ref.kind == RowKind::Lane) {
        for (const auto& t : data.tracks)
            for (const auto& l : t.lanes)
                if (l.id == ref.id) { srcTrackId = t.id; break; }
    }

    ro.erase(ro.begin() + from);
    int insertAt = (toGap > from) ? toGap - 1 : toGap;
    ro.insert(ro.begin() + insertAt, ref);

    // When a lane row is dropped under a different track, transfer lane ownership
    if (ref.kind == RowKind::Lane && srcTrackId >= 0) {
        // Find destination track: scan backward, treating instrument headers as hard boundaries.
        // Without this, the scan would silently cross a header and steal the previous track's ID.
        int destTrackId = -1;
        for (int i = insertAt - 1; i >= 0 && destTrackId < 0; i--) {
            if (ro[i].kind == RowKind::Lane) {
                for (const auto& t : data.tracks)
                    for (const auto& l : t.lanes)
                        if (l.id == ro[i].id) { destTrackId = t.id; break; }
            } else if (ro[i].kind == RowKind::Header) {
                destTrackId = ro[i].id;  // landed immediately after this track's header
                break;
            }
        }
        for (int i = insertAt + 1; i < (int)ro.size() && destTrackId < 0; i++)
            if (ro[i].kind == RowKind::Lane)
                for (const auto& t : data.tracks)
                    for (const auto& l : t.lanes)
                        if (l.id == ro[i].id) { destTrackId = t.id; break; }

        if (destTrackId >= 0 && destTrackId != srcTrackId) {
            for (auto& src : data.tracks) {
                if (src.id != srcTrackId) continue;
                // Stacked tracks must always keep at least one lane
                if (src.stackedLanes && src.lanes.size() <= 1) break;
                auto it = std::find_if(src.lanes.begin(), src.lanes.end(),
                    [&ref](const Lane& l) { return l.id == ref.id; });
                if (it == src.lanes.end()) break;
                Lane lane = std::move(*it);
                src.lanes.erase(it);
                for (auto& dst : data.tracks)
                    if (dst.id == destTrackId) { dst.lanes.push_back(std::move(lane)); break; }
                break;
            }
        }

        // Sync the affected track's lanes[] order to match the new rowOrder sequence.
        // This keeps laneNum (derived from lanes[] index) consistent with visual position,
        // so unnamed lanes display as "Track 1", "Track 2", etc. in their new order.
        int syncId = (destTrackId >= 0 && destTrackId != srcTrackId) ? destTrackId : srcTrackId;
        for (auto& t : data.tracks) {
            if (t.id != syncId) continue;
            std::unordered_map<int, Lane> byId;
            for (auto& l : t.lanes) byId[l.id] = std::move(l);
            t.lanes.clear();
            for (const auto& r : ro)
                if (r.kind == RowKind::Lane && byId.count(r.id))
                    t.lanes.push_back(std::move(byId[r.id]));
            break;
        }

    } else if (ref.kind == RowKind::Param) {
        // A param row belongs to the track it sits under: scan backward to the nearest
        // lane/header, then re-point the lane to that track's instrument so it routes there.
        int destTrackId = -1;
        for (int i = insertAt - 1; i >= 0 && destTrackId < 0; i--) {
            if (ro[i].kind == RowKind::Lane) {
                destTrackId = trackIdForLaneId(ro[i].id);
            } else if (ro[i].kind == RowKind::Header) {
                destTrackId = ro[i].id;
                break;
            }
        }
        if (destTrackId >= 0) {
            int instrId = 0;
            for (const auto& t : data.tracks)
                if (t.id == destTrackId) { instrId = t.instrumentId; break; }
            for (auto& lane : data.paramLanes)
                if (lane.id == ref.id) { lane.instrumentId = instrId; break; }
        }
    }

    rebuildInstrumentHeaders();
    notify();
}

void ObservableSong::moveTrack(int trackId, int insertBeforeTrackId)
{
    if (trackId == insertBeforeTrackId) return;

    auto& ro = data.rowOrder;

    // Collect lane IDs belonging to the track being moved
    std::set<int> moveLaneIds;
    for (const auto& t : data.tracks)
        if (t.id == trackId)
            for (const auto& l : t.lanes) moveLaneIds.insert(l.id);

    // Split rowOrder into the track's block and everything else
    std::vector<RowRef> block, rest;
    for (const auto& r : ro) {
        bool mine = (r.kind == RowKind::Header && r.id == trackId) ||
                    (r.kind == RowKind::Lane   && moveLaneIds.count(r.id));
        (mine ? block : rest).push_back(r);
    }
    if (block.empty()) return;

    // Find the insertion point in `rest` (before insertBeforeTrackId's first row)
    int insertAt = (int)rest.size();  // default: append at end
    if (insertBeforeTrackId >= 0) {
        std::set<int> targetLaneIds;
        for (const auto& t : data.tracks)
            if (t.id == insertBeforeTrackId)
                for (const auto& l : t.lanes) targetLaneIds.insert(l.id);
        for (int i = 0; i < (int)rest.size(); i++) {
            if ((rest[i].kind == RowKind::Header && rest[i].id == insertBeforeTrackId) ||
                (rest[i].kind == RowKind::Lane   && targetLaneIds.count(rest[i].id))) {
                insertAt = i;
                break;
            }
        }
    }

    // Rebuild rowOrder with the block at the new position
    ro.clear();
    for (int i = 0; i <= (int)rest.size(); i++) {
        if (i == insertAt)
            for (const auto& r : block) ro.push_back(r);
        if (i < (int)rest.size())
            ro.push_back(rest[i]);
    }

    // Reorder data.tracks to match the new rowOrder sequence
    int selectedTrackId = (data.selectedTrackIndex >= 0 && data.selectedTrackIndex < (int)data.tracks.size())
        ? data.tracks[data.selectedTrackIndex].id : -1;

    std::unordered_map<int, Track> byId;
    for (auto& t : data.tracks) byId[t.id] = std::move(t);
    data.tracks.clear();
    std::set<int> seen;
    for (const auto& r : ro) {
        int tid = -1;
        if (r.kind == RowKind::Header) {
            tid = r.id;
        } else if (r.kind == RowKind::Lane) {
            for (const auto& [id, t] : byId)
                for (const auto& l : t.lanes)
                    if (l.id == r.id) { tid = id; break; }
        }
        if (tid >= 0 && !seen.count(tid)) {
            seen.insert(tid);
            if (byId.count(tid)) data.tracks.push_back(std::move(byId[tid]));
        }
    }
    for (auto& [id, t] : byId)
        if (!seen.count(id)) data.tracks.push_back(std::move(t));

    // Restore selectedTrackIndex
    data.selectedTrackIndex = -1;
    for (int i = 0; i < (int)data.tracks.size(); i++)
        if (data.tracks[i].id == selectedTrackId) { data.selectedTrackIndex = i; break; }

    rebuildInstrumentHeaders();
    notify();
}

void ObservableSong::setTrackSolo(int trackId, bool s)
{
    for (auto& t : data.tracks)
        if (t.id == trackId) { t.solo = s; notify(); return; }
}

void ObservableSong::setTrackMute(int trackId, bool m)
{
    for (auto& t : data.tracks)
        if (t.id == trackId) { t.mute = m; notify(); return; }
}

bool ObservableSong::isTrackPlaying(int trackId) const
{
    bool anySolo = false;
    for (const auto& t : data.tracks) if (t.solo) { anySolo = true; break; }
    for (const auto& t : data.tracks) {
        if (t.id != trackId) continue;
        return !t.mute && (!anySolo || t.solo);
    }
    return false;
}

void ObservableSong::selectTrack(int index)
{
    data.selectedTrackIndex = index;
    if (index >= 0 && index < (int)data.tracks.size() && !data.tracks[index].lanes.empty())
        data.selectedLaneId = data.tracks[index].lanes[0].id;
    else
        data.selectedLaneId = -1;
    notify();
}

void ObservableSong::selectLane(int trackIndex, int laneId)
{
    data.selectedTrackIndex = trackIndex;
    data.selectedLaneId     = laneId;
    notify();
}

int ObservableSong::addLane(int trackId)
{
    for (auto& t : data.tracks) {
        if (t.id != trackId) continue;

        PatternType ptype  = PatternType::STANDARD;
        float       beats  = 8.0f;
        int         instrId = defaultInstrumentId;
        if (!t.lanes.empty()) {
            for (const auto& p : data.patterns)
                if (p.id == t.lanes[0].patternId) {
                    ptype   = p.type;
                    beats   = p.lengthBeats;
                    instrId = p.instrumentId;
                    break;
                }
        }

        std::set<int> existingLaneIds;
        for (const auto& l : t.lanes) existingLaneIds.insert(l.id);

        int patId = nextId++;
        Pattern newPat;
        newPat.id           = patId;
        newPat.lengthBeats  = beats;
        newPat.name         = "Pattern " + std::to_string(nextPatternNumber++);
        newPat.type         = ptype;
        newPat.instrumentId = instrId;
        data.patterns.push_back(std::move(newPat));

        int laneId = nextId++;
        t.lanes.push_back(Lane{laneId, patId, {}});

        if (!t.stackedLanes || existingLaneIds.empty()) {
            int insertAt = (int)data.rowOrder.size();
            if (existingLaneIds.empty()) {
                // Track was empty — insert after its header row to keep it in place
                for (int i = 0; i < (int)data.rowOrder.size(); i++)
                    if (data.rowOrder[i].kind == RowKind::Header && data.rowOrder[i].id == trackId)
                        { insertAt = i + 1; break; }
            } else {
                for (int i = (int)data.rowOrder.size() - 1; i >= 0; i--)
                    if (data.rowOrder[i].kind == RowKind::Lane && existingLaneIds.count(data.rowOrder[i].id))
                        { insertAt = i + 1; break; }
            }
            data.rowOrder.insert(data.rowOrder.begin() + insertAt, RowRef{RowKind::Lane, laneId});
        }
        rebuildInstrumentHeaders();
        notify();
        return laneId;
    }
    return -1;
}

int ObservableSong::addPianorollLane(int trackId)
{
    for (auto& t : data.tracks) {
        if (t.id != trackId) continue;

        float beats   = 8.0f;
        int   instrId = defaultInstrumentId;
        if (!t.lanes.empty()) {
            for (const auto& p : data.patterns)
                if (p.id == t.lanes[0].patternId) {
                    beats   = p.lengthBeats;
                    instrId = p.instrumentId;
                    break;
                }
        }

        std::set<int> existingLaneIds;
        for (const auto& l : t.lanes) existingLaneIds.insert(l.id);

        int patId = nextId++;
        Pattern newPat;
        newPat.id           = patId;
        newPat.lengthBeats  = beats;
        newPat.name         = "Pattern " + std::to_string(nextPatternNumber++);
        newPat.type         = PatternType::PIANOROLL;
        newPat.instrumentId = instrId;
        data.patterns.push_back(std::move(newPat));

        int laneId = nextId++;
        t.lanes.push_back(Lane{laneId, patId, {}});

        if (!t.stackedLanes || existingLaneIds.empty()) {
            int insertAt = (int)data.rowOrder.size();
            if (existingLaneIds.empty()) {
                for (int i = 0; i < (int)data.rowOrder.size(); i++)
                    if (data.rowOrder[i].kind == RowKind::Header && data.rowOrder[i].id == trackId)
                        { insertAt = i + 1; break; }
            } else {
                for (int i = (int)data.rowOrder.size() - 1; i >= 0; i--)
                    if (data.rowOrder[i].kind == RowKind::Lane && existingLaneIds.count(data.rowOrder[i].id))
                        { insertAt = i + 1; break; }
            }
            data.rowOrder.insert(data.rowOrder.begin() + insertAt, RowRef{RowKind::Lane, laneId});
        }
        rebuildInstrumentHeaders();
        notify();
        return laneId;
    }
    return -1;
}

void ObservableSong::setPatternName(int patId, std::string name)
{
    auto first = name.find_first_not_of(" \t\r\n");
    auto last  = name.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    name = name.substr(first, last - first + 1);
    for (auto& p : data.patterns)
        if (p.id == patId) { p.name = std::move(name); notify(); return; }
}

void ObservableSong::setStackedLanes(int trackId, bool stacked)
{
    for (auto& t : data.tracks) {
        if (t.id != trackId) continue;
        if (t.stackedLanes == stacked) return;
        if (stacked && t.lanes.empty()) return;
        t.stackedLanes = stacked;

        if (stacked) {
            // Remove non-first lane RowRefs from rowOrder
            std::set<int> collapsedIds;
            for (int j = 1; j < (int)t.lanes.size(); j++)
                collapsedIds.insert(t.lanes[j].id);
            data.rowOrder.erase(
                std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
                    [&collapsedIds](const RowRef& r) { return r.kind == RowKind::Lane && collapsedIds.count(r.id); }),
                data.rowOrder.end());
            // Reset selectedLaneId to first lane so a row is highlighted
            if (!t.lanes.empty()) {
                bool selIsNonFirst = false;
                for (int j = 1; j < (int)t.lanes.size(); j++)
                    if (t.lanes[j].id == data.selectedLaneId) { selIsNonFirst = true; break; }
                if (selIsNonFirst) data.selectedLaneId = t.lanes[0].id;
            }
        } else {
            // Re-insert non-first lane RowRefs after the first lane's RowRef
            int insertAt = (int)data.rowOrder.size();
            if (!t.lanes.empty()) {
                int firstId = t.lanes[0].id;
                for (int i = 0; i < (int)data.rowOrder.size(); i++)
                    if (data.rowOrder[i].kind == RowKind::Lane && data.rowOrder[i].id == firstId)
                        { insertAt = i + 1; break; }
            }
            for (int j = 1; j < (int)t.lanes.size(); j++)
                data.rowOrder.insert(data.rowOrder.begin() + insertAt + (j - 1), RowRef{RowKind::Lane, t.lanes[j].id});
        }
        rebuildInstrumentHeaders();
        notify();
        return;
    }
}

void ObservableSong::removeLane(int trackId, int laneId)
{
    for (auto& t : data.tracks) {
        if (t.id != trackId) continue;
        // Stacked tracks always need at least one lane to show a row for pattern instances
        if (t.stackedLanes && (int)t.lanes.size() <= 1) return;

        int patId = -1;
        for (const auto& l : t.lanes)
            if (l.id == laneId) { patId = l.patternId; break; }

        t.lanes.erase(
            std::remove_if(t.lanes.begin(), t.lanes.end(),
                [laneId](const Lane& l) { return l.id == laneId; }),
            t.lanes.end());
        data.rowOrder.erase(
            std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
                [laneId](const RowRef& r) { return r.kind == RowKind::Lane && r.id == laneId; }),
            data.rowOrder.end());

        if (patId > 0 && !patternStillReferenced(patId))
            data.patterns.erase(
                std::remove_if(data.patterns.begin(), data.patterns.end(),
                    [patId](const Pattern& p) { return p.id == patId; }),
                data.patterns.end());

        if (t.lanes.empty())
            t.stackedLanes = false;

        if (data.selectedLaneId == laneId) {
            if (!t.lanes.empty()) {
                data.selectedLaneId = t.lanes[0].id;
                for (int i = 0; i < (int)data.tracks.size(); i++)
                    if (data.tracks[i].id == trackId) { data.selectedTrackIndex = i; break; }
            } else {
                data.selectedLaneId = -1;
            }
        }
        rebuildInstrumentHeaders();
        notify();
        return;
    }
}

void ObservableSong::removeTrack(int trackId)
{
    std::set<int> laneIds;
    int instrumentId = 0;
    for (const auto& t : data.tracks)
        if (t.id == trackId) {
            instrumentId = t.instrumentId;
            for (const auto& l : t.lanes) laneIds.insert(l.id);
        }
    removeParamLanesForInstrument(instrumentId);

    data.tracks.erase(
        std::remove_if(data.tracks.begin(), data.tracks.end(),
            [trackId](const Track& t) { return t.id == trackId; }),
        data.tracks.end());
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [&laneIds, trackId](const RowRef& r) {
                return (r.kind == RowKind::Lane   && laneIds.count(r.id)) ||
                       (r.kind == RowKind::Header && r.id == trackId);
            }),
        data.rowOrder.end());
    if (laneIds.count(data.selectedLaneId)) {
        int idx = data.selectedTrackIndex;
        idx = std::clamp(idx, 0, std::max(0, (int)data.tracks.size() - 1));
        data.selectedTrackIndex = idx;
        if (idx < (int)data.tracks.size() && !data.tracks[idx].lanes.empty())
            data.selectedLaneId = data.tracks[idx].lanes[0].id;
        else
            data.selectedLaneId = -1;
    }
    notify();
}

int ObservableSong::nextTrackNumberForType(PatternType type) const
{
    int count = 0;
    for (const auto& t : data.tracks) {
        if (t.lanes.empty()) continue;
        for (const auto& p : data.patterns) {
            if (p.id == t.lanes[0].patternId && p.type == type) { ++count; break; }
        }
    }
    return count + 1;
}

void ObservableSong::removeTrackAndPattern(int trackId)
{
    auto it = std::find_if(data.tracks.begin(), data.tracks.end(),
        [trackId](const Track& t) { return t.id == trackId; });
    if (it == data.tracks.end()) return;

    std::set<int> laneIds;
    for (const auto& l : it->lanes) laneIds.insert(l.id);
    int patId = it->lanes.empty() ? 0 : it->lanes[0].patternId;
    removeParamLanesForInstrument(it->instrumentId);
    data.tracks.erase(it);
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [&laneIds, trackId](const RowRef& r) {
                return (r.kind == RowKind::Lane   && laneIds.count(r.id)) ||
                       (r.kind == RowKind::Header && r.id == trackId);
            }),
        data.rowOrder.end());

    if (patId > 0 && !patternStillReferenced(patId))
        data.patterns.erase(std::remove_if(data.patterns.begin(), data.patterns.end(),
            [patId](const Pattern& p) { return p.id == patId; }), data.patterns.end());

    if (data.selectedTrackIndex >= (int)data.tracks.size())
        data.selectedTrackIndex = std::max(0, (int)data.tracks.size() - 1);
    {
        int idx = data.selectedTrackIndex;
        if (idx >= 0 && idx < (int)data.tracks.size() && !data.tracks[idx].lanes.empty())
            data.selectedLaneId = data.tracks[idx].lanes[0].id;
        else
            data.selectedLaneId = -1;
    }
    notify();
}

const PatternInstance* ObservableSong::instanceById(int instanceId) const
{
    for (const auto& track : data.tracks)
        for (const auto& lane : track.lanes)
            for (const auto& inst : lane.patterns)
                if (inst.id == instanceId)
                    return &inst;
    return nullptr;
}

int ObservableSong::laneIdForInstance(int instanceId) const
{
    for (const auto& track : data.tracks)
        for (const auto& lane : track.lanes)
            for (const auto& inst : lane.patterns)
                if (inst.id == instanceId)
                    return lane.id;
    return -1;
}

// A Pattern is reachable two independent ways: a Lane.patternId (the pattern
// open in the editor / named in the song-editor left column) and each placed
// PatternInstance.patternId. Pattern GC MUST check both — dropping a pattern
// that a lane still points at orphans the lane (blank name, can't add notes).
// Route every "is this pattern still used?" check through here so neither kind
// is forgotten.
bool ObservableSong::patternStillReferenced(int patId) const
{
    for (const auto& t : data.tracks)
        for (const auto& l : t.lanes) {
            if (l.patternId == patId) return true;          // lane's editor pattern
            for (const auto& p : l.patterns)
                if (p.patternId == patId) return true;      // placed instance
        }
    return false;
}

const Pattern* ObservableSong::patternForInstance(int instanceId) const
{
    for (const auto& track : data.tracks)
        for (const auto& lane : track.lanes)
            for (const auto& inst : lane.patterns)
                if (inst.id == instanceId) {
                    for (const auto& pat : data.patterns)
                        if (pat.id == inst.patternId)
                            return &pat;
                    return nullptr;
                }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Pattern instance management

void ObservableSong::addPattern(int trackIndex, float startBar, float length, float patternBeats)
{
    if (trackIndex < 0 || trackIndex >= (int)data.tracks.size()) return;
    if (data.tracks[trackIndex].lanes.empty()) return;
    int patId = 0;
    if (patternBeats > 0.0f) {
        patId = nextId++;
        Pattern newPat;
        newPat.id           = patId;
        newPat.lengthBeats  = patternBeats;
        newPat.instrumentId = data.tracks[trackIndex].instrumentId;  // route to the track's instrument
        data.patterns.push_back(std::move(newPat));
    }
    data.tracks[trackIndex].lanes[0].patterns.push_back({nextId++, patId, startBar, length});
    notify();
}

void ObservableSong::removePattern(int instanceId)
{
    for (auto& track : data.tracks) {
        for (auto& lane : track.lanes) {
            auto it = std::find_if(lane.patterns.begin(), lane.patterns.end(),
                [instanceId](const PatternInstance& p) { return p.id == instanceId; });
            if (it != lane.patterns.end()) {
                int patId = it->patternId;
                lane.patterns.erase(it);
                if (patId > 0 && !patternStillReferenced(patId))
                    data.patterns.erase(std::remove_if(data.patterns.begin(), data.patterns.end(),
                        [patId](const Pattern& p) { return p.id == patId; }), data.patterns.end());
                notify();
                return;
            }
        }
    }
}

void ObservableSong::movePattern(int instanceId, int newLaneId, float newStartBar)
{
    Lane* dest = nullptr;
    for (auto& t : data.tracks)
        for (auto& l : t.lanes)
            if (l.id == newLaneId) { dest = &l; break; }
    if (!dest) return;
    for (auto& track : data.tracks) {
        for (auto& lane : track.lanes) {
            auto it = std::find_if(lane.patterns.begin(), lane.patterns.end(),
                [instanceId](const PatternInstance& p) { return p.id == instanceId; });
            if (it != lane.patterns.end()) {
                PatternInstance inst = *it;
                lane.patterns.erase(it);
                inst.startBar = newStartBar;
                dest->patterns.push_back(std::move(inst));
                notify();
                return;
            }
        }
    }
}

void ObservableSong::resizePattern(int patternId, float newLength)
{
    for (auto& track : data.tracks) {
        for (auto& lane : track.lanes) {
            for (auto& p : lane.patterns) {
                if (p.id == patternId) {
                    p.length = newLength;
                    notify();
                    return;
                }
            }
        }
    }
}

void ObservableSong::resizePatternLeft(int patternId, float newStartBar, float newLength, float newStartOffset)
{
    for (auto& track : data.tracks)
        for (auto& lane : track.lanes)
            for (auto& p : lane.patterns)
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
        for (auto& lane : track.lanes)
            for (auto& p : lane.patterns)
                if (p.id == patternId) { p.startOffset = startOffset; notify(); return; }
}

void ObservableSong::placePattern(int laneId, int patternId, float startBar, float length)
{
    for (auto& t : data.tracks)
        for (auto& l : t.lanes)
            if (l.id == laneId) {
                l.patterns.push_back({nextId++, patternId, startBar, length});
                notify();
                return;
            }
}

std::vector<Note> ObservableSong::buildNotes() const
{
    std::vector<Note> notes;
    for (int row = 0; row < (int)data.rowOrder.size(); row++) {
        const auto& ref = data.rowOrder[row];
        if (ref.kind != RowKind::Lane) continue;
        for (const auto& t : data.tracks) {
            for (const auto& lane : t.lanes) {
                if (lane.id != ref.id) continue;
                bool isFirstLane = (!t.lanes.empty() && t.lanes[0].id == ref.id);
                if (t.stackedLanes && isFirstLane) {
                    // Stacked: all lanes' instances at this row
                    for (const auto& l : t.lanes)
                        for (const auto& p : l.patterns)
                            notes.push_back({p.id, row, p.startBar, p.length});
                } else {
                    for (const auto& p : lane.patterns)
                        notes.push_back({p.id, row, p.startBar, p.length});
                }
                goto nextRow;
            }
        }
        nextRow:;
    }
    return notes;
}

bool ObservableSong::hasParamLane(const std::string& type, int instrumentId) const
{
    for (const auto& lane : data.paramLanes)
        if (lane.type == type && lane.instrumentId == instrumentId) return true;
    return false;
}

int ObservableSong::addParamLane(const std::string& type, int instrumentId, int atIndex)
{
    int laneId = nextId++;
    ParamLane lane;
    lane.id           = laneId;
    lane.type         = type;
    lane.instrumentId = instrumentId;
    lane.points.push_back({nextId++, 0.0f, laneDefaultValue(type), true});
    data.paramLanes.push_back(std::move(lane));
    RowRef ref{RowKind::Param, laneId};
    if (atIndex >= 0 && atIndex <= (int)data.rowOrder.size())
        data.rowOrder.insert(data.rowOrder.begin() + atIndex, ref);
    else
        data.rowOrder.push_back(ref);
    notify();
    return laneId;
}

int ObservableSong::instrumentIdForParamLane(int laneId) const
{
    for (const auto& lane : data.paramLanes)
        if (lane.id == laneId) return lane.instrumentId;
    return 0;
}

// Drop every param lane owned by an instrument (and its rowOrder rows). Does not
// notify — call from a method that already notifies. instrumentId 0 is a no-op.
void ObservableSong::removeParamLanesForInstrument(int instrumentId)
{
    if (instrumentId == 0) return;
    std::set<int> laneIds;
    for (const auto& lane : data.paramLanes)
        if (lane.instrumentId == instrumentId) laneIds.insert(lane.id);
    if (laneIds.empty()) return;
    data.paramLanes.erase(
        std::remove_if(data.paramLanes.begin(), data.paramLanes.end(),
            [instrumentId](const ParamLane& l) { return l.instrumentId == instrumentId; }),
        data.paramLanes.end());
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [&laneIds](const RowRef& r) {
                return r.kind == RowKind::Param && laneIds.count(r.id); }),
        data.rowOrder.end());
}

void ObservableSong::removeParamLane(int laneId)
{
    data.paramLanes.erase(
        std::remove_if(data.paramLanes.begin(), data.paramLanes.end(),
            [laneId](const ParamLane& l) { return l.id == laneId; }),
        data.paramLanes.end());
    data.rowOrder.erase(
        std::remove_if(data.rowOrder.begin(), data.rowOrder.end(),
            [laneId](const RowRef& r) { return r.kind == RowKind::Param && r.id == laneId; }),
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
