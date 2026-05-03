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

void ObservablePattern::addNote(int patternId, float start, int pitch, float length, float velocity)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id == patternId) {
            pat.notes.push_back({song_->nextId++, pitch, start, length, velocity});
            song_->notify();
            return;
        }
    }
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
                n.pitch = (int)newPitch;
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

std::vector<Note> ObservablePattern::buildPatternNotes(int patternId) const
{
    for (const auto& pat : song_->data.patterns)
        if (pat.id == patternId)
            return pat.notes;
    return {};
}

void ObservablePattern::remapPatternNotes(int patId, int oldSize, int newSize)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id != patId) continue;
        for (auto& note : pat.notes) {
            if (note.disabled) {
                if (note.disabledDegree >= 0 && note.disabledDegree < newSize) {
                    note.pitch    = note.pitch * newSize + note.disabledDegree;
                    note.disabled = false;
                    note.disabledDegree = -1;
                }
            } else {
                int degree = note.pitch % oldSize;
                int octave = note.pitch / oldSize;
                if (degree < newSize) {
                    note.pitch = octave * newSize + degree;
                } else {
                    note.disabled       = true;
                    note.disabledDegree = degree;
                    note.pitch          = octave;
                }
            }
        }
        break;
    }
    song_->notify();
}

// ---------------------------------------------------------------------------
// Drum note CRUD

void ObservablePattern::addDrumNote(int patternId, int note, float beat, float velocity)
{
    for (auto& pat : song_->data.patterns) {
        if (pat.id == patternId) {
            pat.drumNotes.push_back({song_->nextId++, note, beat, velocity});
            song_->notify();
            return;
        }
    }
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

std::vector<DrumNote> ObservablePattern::buildDrumPatternNotes(int patternId) const
{
    for (const auto& pat : song_->data.patterns)
        if (pat.id == patternId)
            return pat.drumNotes;
    return {};
}

// ---------------------------------------------------------------------------
// Pattern lifecycle

int ObservablePattern::createPattern(float lengthBeats)
{
    int id = song_->nextId++;
    Pattern p;
    p.id = id;
    p.lengthBeats = lengthBeats;
    p.outputInstrumentName = song_->defaultOutputInstrument;
    song_->timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    song_->data.patterns.push_back(p);
    song_->notify();
    return id;
}

int ObservablePattern::createDrumPattern(float lengthBeats)
{
    int id = song_->nextId++;
    Pattern p;
    p.id = id;
    p.lengthBeats = lengthBeats;
    p.type = PatternType::DRUM;
    p.outputInstrumentName = song_->defaultDrumOutputInstrument.empty()
        ? song_->defaultOutputInstrument : song_->defaultDrumOutputInstrument;
    song_->timeSigAt(0, p.timeSigTop, p.timeSigBottom);
    song_->data.patterns.push_back(std::move(p));
    song_->notify();
    return id;
}

int ObservablePattern::createPianorollPattern(float lengthBeats)
{
    int id = song_->nextId++;
    Pattern p;
    p.id = id;
    p.lengthBeats = lengthBeats;
    p.type = PatternType::PIANOROLL;
    p.outputInstrumentName = song_->defaultOutputInstrument;
    song_->timeSigAt(0, p.timeSigTop, p.timeSigBottom);
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

void ObservablePattern::setPatternTimeSig(int patId, int top, int bottom)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            int bars = std::max(1, (int)std::round(p.lengthBeats / (float)p.timeSigTop));
            p.timeSigTop    = top;
            p.timeSigBottom = bottom;
            p.lengthBeats   = (float)(bars * top);
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

void ObservablePattern::setPatternOutputInstrument(int patId, const std::string& instrumentName)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patId) {
            p.outputInstrumentName = instrumentName;
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
