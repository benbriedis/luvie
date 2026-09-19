// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "observablePattern.hpp"
#include "paramLaneTypes.hpp"
#include <algorithm>
#include <cmath>

ObservablePattern::ObservablePattern(ObservableSong* song)
    : song_(song)
{
    if (song_) song_->addObserver(this);
}

ObservablePattern::~ObservablePattern()
{
    if (song_) song_->removeObserver(this);
}

void ObservablePattern::addObserver(ITimelineObserver* o)
{
    observers_.push_back(o);
}

void ObservablePattern::removeObserver(ITimelineObserver* o)
{
    observers_.erase(std::remove(observers_.begin(), observers_.end(), o), observers_.end());
}

void ObservablePattern::notify()
{
    auto copy = observers_;
    for (auto* o : copy) o->onTimelineChanged();
}

void ObservablePattern::onTimelineChanged()
{
    notify();
}

// ---------------------------------------------------------------------------
// Note CRUD

int ObservablePattern::addNote(int patternId, float start, int pitch, float length, float velocity)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id == patternId) {
            int id = song_->nextId++;
            pat.notes.push_back({id, pitch, start, length, velocity});
            song_->notify();
            return id;
        }
    }
    return 0;
}

int ObservablePattern::addBonusNote(int patternId, float start, int pitchGroup, int bonusDegree,
                                    float length, float velocity)
{
    if (bonusDegree < 0) return 0;
    for (auto& pat : song_->data.patterns) {
        if (pat.id == patternId) {
            int id = song_->nextId++;
            pat.notes.push_back({id, pitchGroup, start, length, velocity,
                                 true, bonusDegree});
            song_->notify();
            return id;
        }
    }
    return 0;
}

void ObservablePattern::removeNote(int noteId)
{
    for (auto& pat : song_->data.patterns) {
        auto it = std::find_if(pat.notes.begin(), pat.notes.end(),
            [noteId](const Note& n) { return n.id == noteId; });
        if (it != pat.notes.end()) {
            pat.notes.erase(it);
            song_->notify();
            return;
        }
    }
}

void ObservablePattern::moveNote(int noteId, float newStart, float newPitch)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.notes) {
            if (n.id == noteId) {
                n.beat  = newStart;
                n.row = (int)newPitch;
                song_->notify();
                return;
            }
        }
    }
}

void ObservablePattern::resizeNoteRight(int noteId, float newLength)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.notes) {
            if (n.id == noteId) {
                n.length = newLength;
                song_->notify();
                return;
            }
        }
    }
}

void ObservablePattern::resizeNoteLeft(int noteId, float newStart, float newLength)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.notes) {
            if (n.id == noteId) {
                n.beat   = newStart;
                n.length = newLength;
                song_->notify();
                return;
            }
        }
    }
}

void ObservablePattern::setNoteVelocity(int noteId, float velocity)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.notes) {
            if (n.id == noteId) {
                n.velocity = std::clamp(velocity, 0.0f, 1.0f);
                song_->notify();
                return;
            }
        }
    }
}

// A drag in the harmony editor changes the beat and the row together, and the
// row may switch a note between ordinary and bonus — so it goes through one
// call, and one notify(), rather than a moveNote() plus a row assignment.
void ObservablePattern::moveNoteToSlot(float newStart, const NoteRowSlot& slot)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.notes) {
            if (n.id != slot.noteId) continue;
            n.beat        = newStart;
            n.row         = slot.row;
            n.bonus       = slot.bonus;
            n.bonusDegree = slot.bonusDegree;
            song_->notify();
            return;
        }
    }
}

std::vector<Note> ObservablePattern::buildPatternNotes(int patternId) const
{
    for (const auto& pat : song_->data.patterns)
        if (pat.id == patternId)
            return pat.notes;
    return {};
}

// Place a harmony note given as (pitch group, degree) — coordinates that mean
// the same in every chord — into the row encoding of a chord of `chordSize`
// degrees. A degree the chord has no room for becomes a bonus note, which keeps
// it so a later, bigger chord can take it back.
static void encodeHarmonyNote(Note& note, int pitchGroup, int degree, int chordSize)
{
    if (degree < chordSize) {
        note.row         = pitchGroup * chordSize + degree;
        note.bonus       = false;
        note.bonusDegree = -1;
    } else {
        note.row         = pitchGroup;
        note.bonus       = true;
        note.bonusDegree = degree;
    }
}

// The inverse: a stored harmony note's pitch group and degree.
static void decodeHarmonyNote(const Note& note, int chordSize, int& pitchGroup, int& degree)
{
    if (note.bonus) { pitchGroup = note.row; degree = note.bonusDegree; }
    else            { pitchGroup = note.row / chordSize; degree = note.row % chordSize; }
}

void ObservablePattern::remapPatternNotes(int patId, int oldSize, int newSize)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patId) continue;
        for (auto& note : pat.notes) {
            // A bonus note whose degree is out of range has nowhere to go back to.
            if (note.bonus && note.bonusDegree < 0) continue;
            int pitchGroup, degree;
            decodeHarmonyNote(note, oldSize, pitchGroup, degree);
            encodeHarmonyNote(note, pitchGroup, degree, newSize);
        }
        break;
    }
    song_->notify();
}

// ---------------------------------------------------------------------------
// Drum note CRUD

int ObservablePattern::addDrumNote(int patternId, int note, float beat, float velocity)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id == patternId) {
            int id = song_->nextId++;
            pat.drumNotes.push_back({id, note, beat, velocity});
            song_->notify();
            return id;
        }
    }
    return 0;
}

void ObservablePattern::removeDrumNote(int drumNoteId)
{
    for (auto& pat : song_->data.patterns) {
        auto it = std::find_if(pat.drumNotes.begin(), pat.drumNotes.end(),
            [drumNoteId](const DrumNote& n) { return n.id == drumNoteId; });
        if (it != pat.drumNotes.end()) {
            pat.drumNotes.erase(it);
            song_->notify();
            return;
        }
    }
}

// Move in both axes, keeping the note's id. The drum grid used to commit a drag
// as removeDrumNote + addDrumNote, which issued a NEW id — two notifications for
// one gesture, two undo steps, and no stable identity for a selection to hold on
// to across the drag.
void ObservablePattern::moveDrumNote(int drumNoteId, int note, float beat)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.drumNotes) {
            if (n.id == drumNoteId) {
                n.note = std::clamp(note, 0, 127);
                n.beat = beat;
                song_->notify();
                return;
            }
        }
    }
}

void ObservablePattern::setDrumNoteVelocity(int drumNoteId, float velocity)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& n : pat.drumNotes) {
            if (n.id == drumNoteId) {
                n.velocity = std::clamp(velocity, 0.0f, 1.0f);
                song_->notify();
                return;
            }
        }
    }
}

std::vector<DrumNote> ObservablePattern::buildDrumPatternNotes(int patternId) const
{
    for (const auto& pat : song_->data.patterns)
        if (pat.id == patternId)
            return pat.drumNotes;
    return {};
}

void ObservablePattern::setDrumNoteSolo(int patternId, int note, bool s)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patternId) continue;
        if (s) pat.drumSolo.insert(note);
        else   pat.drumSolo.erase(note);
        song_->notify();
        return;
    }
}

void ObservablePattern::setDrumNoteMute(int patternId, int note, bool m)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patternId) continue;
        if (m) pat.drumMute.insert(note);
        else   pat.drumMute.erase(note);
        song_->notify();
        return;
    }
}

// ---------------------------------------------------------------------------
// Pattern lifecycle

int ObservablePattern::createHarmonyPattern(float lengthBeats, int instrumentId)
{
    int id = song_->nextId++;
    Pattern p;
    p.id           = id;
    p.lengthBeats  = lengthBeats;
    p.instrumentId = instrumentId >= 0 ? instrumentId : song_->defaultInstrumentId;
    song_->patternNames.assignAuto(p);
    song_->timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    p.beat = song_->beatAt(0);
    song_->data.patterns.push_back(p);
    song_->notify();
    return id;
}

int ObservablePattern::createDrumPattern(float lengthBeats, int instrumentId)
{
    int id = song_->nextId++;
    Pattern p;
    p.id           = id;
    p.lengthBeats  = lengthBeats;
    p.type         = PatternType::DRUM;
    p.instrumentId = instrumentId >= 0 ? instrumentId
        : (song_->defaultDrumInstrumentId
            ? song_->defaultDrumInstrumentId : song_->defaultInstrumentId);
    song_->patternNames.assignAuto(p);
    song_->timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    p.beat = song_->beatAt(0);
    song_->data.patterns.push_back(std::move(p));
    song_->notify();
    return id;
}

int ObservablePattern::createPianorollPattern(float lengthBeats)
{
    int id = song_->nextId++;
    Pattern p;
    p.id           = id;
    p.lengthBeats  = lengthBeats;
    p.type         = PatternType::PIANOROLL;
    p.instrumentId = song_->defaultInstrumentId;
    song_->patternNames.assignAuto(p);
    song_->timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    p.beat = song_->beatAt(0);
    song_->data.patterns.push_back(std::move(p));
    song_->notify();
    return id;
}

int ObservablePattern::copyPattern(int srcPatId)
{
    const Pattern* src = nullptr;
    for (const auto& p : song_->data.patterns)
        if (p.id == srcPatId) { src = &p; break; }
    if (!src) return -1;

    Pattern copy;
    copy.id = song_->nextId++;
    copy.lengthBeats = src->lengthBeats;
    copy.type = src->type;
    copy.timeSigTop    = src->timeSigTop;
    copy.timeSigBottom = src->timeSigBottom;
    copy.beat          = src->beat;
    copy.rootPitch = src->rootPitch;
    copy.chordHash = src->chordHash;
    copy.useSharp  = src->useSharp;
    copy.divisions   = src->divisions;
    copy.snapEnabled = src->snapEnabled;
    song_->patternNames.assignDerived(copy, src->name);
    for (auto n : src->notes) {
        n.id = song_->nextId++;
        copy.notes.push_back(n);
    }
    for (auto n : src->drumNotes) {
        n.id = song_->nextId++;
        copy.drumNotes.push_back(n);
    }
    song_->data.patterns.push_back(copy);
    return copy.id;
}

// ---------------------------------------------------------------------------
// Pattern properties

static void truncatePatternNotes(Pattern& p)
{
    p.notes.erase(std::remove_if(p.notes.begin(), p.notes.end(),
        [&p](const Note& n) { return n.beat + n.length > p.lengthBeats; }), p.notes.end());
    p.drumNotes.erase(std::remove_if(p.drumNotes.begin(), p.drumNotes.end(),
        [&p](const DrumNote& n) { return n.beat >= p.lengthBeats; }), p.drumNotes.end());
}

void ObservablePattern::setPatternTimeSig(int patId, int top, int bottom,
                                          timeSettings::BeatUnit beat)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            int bars = std::max(1, (int)std::round(p.lengthBeats / (float)p.timeSigTop));
            float denomScale = (p.timeSigBottom > 0) ? (float)bottom / (float)p.timeSigBottom : 1.0f;
            p.timeSigTop    = top;
            p.timeSigBottom = bottom;
            p.beat          = beat;
            p.lengthBeats   = (float)(bars * top);
            if (denomScale != 1.0f) {
                for (auto& n : p.notes) {
                    n.beat   *= denomScale;
                    n.length *= denomScale;
                }
                for (auto& n : p.drumNotes)
                    n.beat *= denomScale;
                for (auto& lane : p.paramLanes)
                    for (auto& pt : lane.points)
                        pt.beat *= denomScale;
            }
            truncatePatternNotes(p);
            song_->notify();
            return;
        }
    }
}

void ObservablePattern::setPatternLength(int patId, float lengthBeats)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.lengthBeats = lengthBeats;
            truncatePatternNotes(p);
            song_->notify();
            return;
        }
    }
}

void ObservablePattern::setPatternHarmony(int patId, int root, std::string chordHash, bool sharp)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.rootPitch = root;
            p.chordHash = std::move(chordHash);
            p.useSharp  = sharp;
            song_->notify();
            return;
        }
    }
}

void ObservablePattern::setPatternDivisions(int patId, int divisions)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.divisions = divisions;
            song_->notifyViewState();
            return;
        }
    }
}

void ObservablePattern::setPatternSnapEnabled(int patId, bool enabled)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.snapEnabled = enabled;
            song_->notifyViewState();
            return;
        }
    }
}

void ObservablePattern::setPatternZoom(int patId, int zoomPct)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.zoomPct = zoomPct;
            song_->notifyViewState();
            return;
        }
    }
}

void ObservablePattern::setPatternInstrument(int patId, int instrumentId)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.instrumentId = instrumentId;
            song_->notify();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Pattern-level param lanes

bool ObservablePattern::hasPatternParamLane(int patId, const std::string& type) const
{
    for (const auto& p : song_->data.patterns) {
        if (p.id != patId) continue;
        for (const auto& lane : p.paramLanes)
            if (lane.type == type) return true;
        return false;
    }
    return false;
}

int ObservablePattern::addPatternParamLane(int patId, const std::string& type)
{
    for (auto& p : song_->data.patterns) {
        if (p.id != patId) continue;
        int laneId = song_->nextId++;
        ParamLane lane;
        lane.id   = laneId;
        lane.type = type;
        lane.points.push_back({song_->nextId++, 0.0f, laneDefaultValue(type), true});
        p.paramLanes.push_back(std::move(lane));
        song_->notify();
        return laneId;
    }
    return -1;
}

void ObservablePattern::removePatternParamLane(int laneId)
{
    for (auto& p : song_->data.patterns) {
        auto it = std::find_if(p.paramLanes.begin(), p.paramLanes.end(),
            [laneId](const ParamLane& l) { return l.id == laneId; });
        if (it != p.paramLanes.end()) {
            p.paramLanes.erase(it);
            song_->notify();
            return;
        }
    }
}

int ObservablePattern::addPatternParamPoint(int patId, int laneId, float beat, int value)
{
    for (auto& p : song_->data.patterns) {
        if (p.id != patId) continue;
        for (auto& lane : p.paramLanes) {
            if (lane.id != laneId) continue;
            int ptId = song_->nextId++;
            auto it = std::lower_bound(lane.points.begin(), lane.points.end(),
                beat, [](const ParamPoint& p, float b) { return p.beat < b; });
            lane.points.insert(it, {ptId, beat, std::clamp(value, 0, laneMaxValue(lane.type)), false});
            song_->notify();
            return ptId;
        }
        break;
    }
    return -1;
}

void ObservablePattern::removeParamPoint(int pointId)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& lane : pat.paramLanes) {
            auto it = std::find_if(lane.points.begin(), lane.points.end(),
                [pointId](const ParamPoint& p) { return p.id == pointId; });
            if (it != lane.points.end() && !it->anchor) {
                lane.points.erase(it);
                song_->notify();
                return;
            }
        }
    }
}

void ObservablePattern::moveParamPoint(int pointId, float beat, int value)
{
    for (auto& pat : song_->data.patterns) {
        for (auto& lane : pat.paramLanes) {
            for (auto& pt : lane.points) {
                if (pt.id != pointId) continue;
                if (!pt.anchor)
                    pt.beat  = std::max(0.0f, beat);
                pt.value = std::clamp(value, 0, laneMaxValue(lane.type));
                std::stable_sort(lane.points.begin(), lane.points.end(),
                    [](const ParamPoint& a, const ParamPoint& b) { return a.beat < b.beat; });
                song_->notify();
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Time slices
//
// A slice holds every item in [start, end): the notes overlapping it (carried
// whole, as the rubber band takes them), and the drum hits and automation points
// lying in it. Automation is moved as points and nothing more: no points are
// added at the edges to keep the curve outside the range as it was, so a slice
// that starts or ends at a different value from its surroundings ramps into and
// out of them, and the user patches the edges up as they see fit.

namespace {

// Same tolerance as Grid::kBeatEpsilon: far below any real subdivision, well
// above the float noise of snapping to thirds and sevenths.
constexpr float kSliceEps = 1e-4f;

// Harmony pitch groups the editor will show: HarmonyLabels stops at ten.
constexpr int kMaxPitchGroups = 10;

bool rangesOverlap(float aStart, float aLen, float bStart, float bLen)
{
    return aStart + kSliceEps < bStart + bLen && bStart + kSliceEps < aStart + aLen;
}

bool inRange(float beat, float start, float end)
{
    return beat >= start - kSliceEps && beat < end - kSliceEps;
}

bool isNoteType(PatternType t) { return t == PatternType::HARMONY || t == PatternType::PIANOROLL; }

// Insert keeping the lane sorted, after any point already on that beat, so two
// points stacked on one beat keep the order they were copied in.
void insertPoint(ParamLane& lane, int id, float beat, int value)
{
    auto it = std::upper_bound(lane.points.begin(), lane.points.end(), beat,
        [](float b, const ParamPoint& p) { return b + kSliceEps < p.beat; });
    lane.points.insert(it, {id, beat, std::clamp(value, 0, laneMaxValue(lane.type)), false});
}

ParamPoint* anchorOf(ParamLane& lane)
{
    for (auto& p : lane.points) if (p.anchor) return &p;
    return nullptr;
}

} // namespace

SliceClip ObservablePattern::captureRange(int patId, float start, float end) const
{
    SliceClip clip;
    const Pattern* pat = song_->patternById(patId);
    if (!pat || !(end > start)) return clip;

    clip.type   = pat->type;
    clip.length = end - start;
    const int chordSize = chordDefForHash(pat->chordHash).size;

    if (isNoteType(pat->type)) {
        for (const Note& n : pat->notes) {
            if (!rangesOverlap(start, clip.length, n.beat, n.length)) continue;
            SliceNote s;
            s.dBeat    = n.beat - start;
            s.length   = n.length;
            s.velocity = n.velocity;
            if (pat->type == PatternType::PIANOROLL) s.row = n.row;
            else {
                if (n.bonus && n.bonusDegree < 0) continue;
                decodeHarmonyNote(n, chordSize, s.pitchGroup, s.degree);
            }
            clip.notes.push_back(s);
        }
    }
    else {
        for (const DrumNote& d : pat->drumNotes)
            if (inRange(d.beat, start, end))
                clip.drums.push_back({d.note, d.beat - start, d.velocity});
    }

    // Every lane is carried, empty or not, so a paste elsewhere creates the
    // lanes the slice covered even where it held no points.
    for (const ParamLane& lane : pat->paramLanes) {
        SliceLane s;
        s.type = lane.type;
        for (const ParamPoint& p : lane.points)
            if (inRange(p.beat, start, end))
                s.points.emplace_back(p.beat - start, p.value);
        clip.lanes.push_back(std::move(s));
    }
    return clip;
}

// Shared by every slice edit, which each notify once at the end. The anchor at
// beat 0 is never removed: every lane must keep one.
static void clearRangeIn(Pattern& pat, float start, float end)
{
    const float len = end - start;
    if (isNoteType(pat.type)) {
        pat.notes.erase(std::remove_if(pat.notes.begin(), pat.notes.end(),
            [&](const Note& n) { return rangesOverlap(start, len, n.beat, n.length); }),
            pat.notes.end());
    }
    else {
        pat.drumNotes.erase(std::remove_if(pat.drumNotes.begin(), pat.drumNotes.end(),
            [&](const DrumNote& d) { return inRange(d.beat, start, end); }),
            pat.drumNotes.end());
    }
    for (ParamLane& lane : pat.paramLanes)
        lane.points.erase(std::remove_if(lane.points.begin(), lane.points.end(),
            [&](const ParamPoint& p) { return !p.anchor && inRange(p.beat, start, end); }),
            lane.points.end());
}

void ObservablePattern::clearRange(int patId, float start, float end)
{
    if (!(end > start)) return;
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patId) continue;
        clearRangeIn(pat, start, end);
        song_->notify();
        return;
    }
}

bool ObservablePattern::rangeFits(int patId, const SliceClip& clip, float at) const
{
    const Pattern* pat = song_->patternById(patId);
    if (!pat || clip.type != pat->type || !(clip.length > 0.0f)) return false;
    if (at < -kSliceEps || at + clip.length > pat->lengthBeats + kSliceEps) return false;

    for (const SliceNote& n : clip.notes) {
        const float b = at + n.dBeat;
        if (b < -kSliceEps || b + n.length > pat->lengthBeats + kSliceEps) return false;
        if (pat->type == PatternType::PIANOROLL) {
            if (n.row < 0 || n.row > 127) return false;
        } else if (n.pitchGroup < 0 || n.pitchGroup >= kMaxPitchGroups || n.degree < 0) {
            return false;
        }
    }
    for (const SliceDrum& d : clip.drums)
        if (d.note < 0 || d.note > 127) return false;
    return true;
}

// The body of a paste, on a pattern already known to take it (rangeFits).
static void pasteRangeIn(Pattern& pat, const SliceClip& clip, float at, int& nextId)
{
    for (const SliceLane& sl : clip.lanes) {
        bool have = std::any_of(pat.paramLanes.begin(), pat.paramLanes.end(),
            [&](const ParamLane& l) { return l.type == sl.type; });
        if (have) continue;
        ParamLane lane;
        lane.id   = nextId++;
        lane.type = sl.type;
        lane.points.push_back({nextId++, 0.0f, laneDefaultValue(sl.type), true});
        pat.paramLanes.push_back(std::move(lane));
    }

    clearRangeIn(pat, at, at + clip.length);

    if (isNoteType(pat.type)) {
        const int chordSize = chordDefForHash(pat.chordHash).size;
        std::vector<Note> placed;
        placed.reserve(clip.notes.size());
        for (const SliceNote& s : clip.notes) {
            Note n{nextId++, s.row, at + s.dBeat, s.length, s.velocity};
            if (pat.type == PatternType::HARMONY)
                encodeHarmonyNote(n, s.pitchGroup, s.degree, chordSize);
            placed.push_back(n);
        }
        // A note carried whole past either edge can still reach a note outside
        // the range. The slice replaces what it lands on, so that note goes too.
        auto sameRow = [](const Note& a, const Note& b) {
            return a.row == b.row && a.bonus == b.bonus && (!a.bonus || a.bonusDegree == b.bonusDegree);
        };
        pat.notes.erase(std::remove_if(pat.notes.begin(), pat.notes.end(),
            [&](const Note& old) {
                return std::any_of(placed.begin(), placed.end(), [&](const Note& p) {
                    return sameRow(old, p) && rangesOverlap(p.beat, p.length, old.beat, old.length);
                });
            }), pat.notes.end());
        pat.notes.insert(pat.notes.end(), placed.begin(), placed.end());
    }
    else {
        for (const SliceDrum& d : clip.drums)
            pat.drumNotes.push_back({nextId++, d.note, at + d.dBeat, d.velocity});
    }

    for (const SliceLane& sl : clip.lanes) {
        auto it = std::find_if(pat.paramLanes.begin(), pat.paramLanes.end(),
            [&](const ParamLane& l) { return l.type == sl.type; });
        if (it == pat.paramLanes.end()) continue;
        ParamLane& lane = *it;
        bool anchorTaken = false;
        for (const auto& [dBeat, value] : sl.points) {
            const float beat = at + dBeat;
            // Beat 0 already has the anchor, which cannot go: the first point
            // landing there becomes its value rather than a second point.
            if (beat <= kSliceEps && !anchorTaken) {
                if (ParamPoint* a = anchorOf(lane)) {
                    a->value = std::clamp(value, 0, laneMaxValue(lane.type));
                    anchorTaken = true;
                    continue;
                }
            }
            insertPoint(lane, nextId++, beat, value);
        }
    }
}

bool ObservablePattern::pasteRange(int patId, const SliceClip& clip, float at)
{
    if (!rangeFits(patId, clip, at)) return false;
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patId) continue;
        pasteRangeIn(pat, clip, at, song_->nextId);
        song_->notify();
        return true;
    }
    return false;
}

bool ObservablePattern::moveRange(int patId, float start, float end, float to)
{
    SliceClip clip = captureRange(patId, start, end);
    if (!rangeFits(patId, clip, to)) return false;
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patId) continue;
        clearRangeIn(pat, start, end);
        pasteRangeIn(pat, clip, to, song_->nextId);
        song_->notify();
        return true;
    }
    return false;
}
