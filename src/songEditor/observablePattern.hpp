#ifndef OBSERVABLE_PATTERN_HPP
#define OBSERVABLE_PATTERN_HPP

#include "itimelineobserver.hpp"
#include "observableSong.hpp"
#include "timeline.hpp"
#include <string>
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

    // Pattern note CRUD
    void addNote(int patternId, float start, int pitch, float length, float velocity = 0.8f);
    void removeNote(int noteId);
    void moveNote(int noteId, float newStart, float newPitch);
    void resizeNoteRight(int noteId, float newLength);
    void resizeNoteLeft(int noteId, float newStart, float newLength);
    void setNoteVelocity(int noteId, float velocity);
    std::vector<Note> buildPatternNotes(int patternId) const;
    void remapPatternNotes(int patId, int oldSize, int newSize);

    // Drum note CRUD
    void addDrumNote(int patternId, int note, float beat, float velocity = 0.8f);
    void removeDrumNote(int drumNoteId);
    void setDrumNoteVelocity(int drumNoteId, float velocity);
    std::vector<DrumNote> buildDrumPatternNotes(int patternId) const;
    void setDrumNoteSolo(int patternId, int note, bool solo);
    void setDrumNoteMute(int patternId, int note, bool mute);

    // Pattern lifecycle (creation/copy)
    // instrumentId defaults to the song's default (drum) instrument when < 0.
    // Passing it explicitly ensures the auto name reflects that instrument.
    int createPattern(float lengthBeats, int instrumentId = -1);
    int createDrumPattern(float lengthBeats, int instrumentId = -1);
    int createPianorollPattern(float lengthBeats);
    int copyPattern(int srcPatId);

    // Pattern properties
    void setPatternTimeSig(int patId, int top, int bottom);
    void setPatternLength(int patId, float lengthBeats);
    void setPatternHarmony(int patId, int root, std::string chordHash, bool sharp, int octaveOffset);
    void setPatternSnap(int patId, int snap);
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
