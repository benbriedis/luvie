// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef OBSERVABLE_PATTERN_HPP
#define OBSERVABLE_PATTERN_HPP

#include "itimelineobserver.hpp"
#include "observableSong.hpp"
#include "timeline.hpp"
#include <string>
#include <utility>
#include <vector>

// Observes ObservableSong and exposes pattern-level editing (notes, drum notes,
// param lanes inside patterns). Re-notifies its own observers on every song change
// so pattern editors only need to register here, not directly with ObservableSong.
class ObservablePattern : public ITimelineObserver {
public:
    explicit ObservablePattern(ObservableSong* song);
    ~ObservablePattern();

    void addObserver(ITimelineObserver* o);
    void removeObserver(ITimelineObserver* o);

    ObservableSong*  song() const { return song_; }
    const Timeline&  get()  const { return song_->get(); }

    // Pattern note CRUD. The two adders return the new note's id, or 0 when there
    // is no such pattern — a paste selects what it just placed, and that is the
    // only way to know what it was.
    int  addNote(int patternId, float start, int pitch, float length, float velocity = 0.8f);
    // A note placed directly on one of the harmony editor's bonus rows: `pitchGroup`
    // and `bonusDegree` are the row's coordinates, as Note documents.
    int  addBonusNote(int patternId, float start, int pitchGroup, int bonusDegree,
                      float length, float velocity = 0.8f);
    void removeNote(int noteId);
    void moveNote(int noteId, float newStart, float newPitch);
    void resizeNoteRight(int noteId, float newLength);
    void resizeNoteLeft(int noteId, float newStart, float newLength);
    void setNoteVelocity(int noteId, float velocity);

    // Where a note sits in the harmony editor's row space. `row` is the abs_row
    // for an ordinary note and the pitch group for a bonus one, as Note documents.
    struct NoteRowSlot {
        int  noteId;
        int  row;
        bool bonus;
        int  bonusDegree;
    };
    // Move one note in both axes at once: `slot` carries the row it lands on, which
    // may be an ordinary row or a bonus one.
    void moveNoteToSlot(float newStart, const NoteRowSlot& slot);
    std::vector<Note> buildPatternNotes(int patternId) const;
    void remapPatternNotes(int patId, int oldSize, int newSize);

    // Drum note CRUD
    int  addDrumNote(int patternId, int note, float beat, float velocity = 0.8f);
    void removeDrumNote(int drumNoteId);
    // Move a drum note without changing its id, so one drag is one edit.
    void moveDrumNote(int drumNoteId, int note, float beat);
    void setDrumNoteVelocity(int drumNoteId, float velocity);
    std::vector<DrumNote> buildDrumPatternNotes(int patternId) const;
    void setDrumNoteSolo(int patternId, int note, bool solo);
    void setDrumNoteMute(int patternId, int note, bool mute);

    // Pattern lifecycle (creation/copy)
    // instrumentId defaults to the song's default (drum) instrument when < 0.
    // Passing it explicitly ensures the auto name reflects that instrument.
    int createHarmonyPattern(float lengthBeats, int instrumentId = -1);
    int createDrumPattern(float lengthBeats, int instrumentId = -1);
    int createPianorollPattern(float lengthBeats);
    int copyPattern(int srcPatId);

    // Pattern properties
    void setPatternTimeSig(int patId, int top, int bottom,
                           timeSettings::BeatUnit beat = timeSettings::beatUnitDefault);
    void setPatternLength(int patId, float lengthBeats);
    void setPatternHarmony(int patId, int root, std::string chordHash, bool sharp);
    void setPatternDivisions(int patId, int divisions);
    void setPatternSnapEnabled(int patId, bool enabled);
    void setPatternZoom(int patId, int zoom);
    void setPatternInstrument(int patId, int instrumentId);

    // Pattern-level param lanes
    bool hasPatternParamLane(int patId, const std::string& type) const;
    int  addPatternParamLane(int patId, const std::string& type);
    void removePatternParamLane(int laneId);
    int  addPatternParamPoint(int patId, int laneId, float beat, int value);
    void removeParamPoint(int pointId);
    void moveParamPoint(int pointId, float beat, int value);

    // ITimelineObserver — forwards all song changes to pattern observers
    void onTimelineChanged() override;

private:
    ObservableSong*              song_;
    std::vector<ITimelineObserver*> observers_;
    void notify();
};

#endif
