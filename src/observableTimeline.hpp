#ifndef OBSERVABLE_TIMELINE_HPP
#define OBSERVABLE_TIMELINE_HPP

#include "itimelineobserver.hpp"
#include "timeline.hpp"
#include <string>
#include <vector>

class ObservableTimeline {
public:
	ObservableTimeline(float initBpm, int initTop, int initBottom);

	void addObserver(ITimelineObserver* o);
	void removeObserver(ITimelineObserver* o);

	const Timeline& get() const { return data; }

	// BPM
	void  setBpm(int bar, float bpm);        // update existing marker or add new one
	void  removeBpm(int bar);                // bar 0 is ignored (fixed)
	void  moveBpmMarker(int fromBar, int toBar);
	float bpmAt(int bar) const;              // effective BPM at or before bar

	// Time signature
	void setTimeSig(int bar, int top, int bottom);
	void removeTimeSig(int bar);
	void moveTimeSigMarker(int fromBar, int toBar);
	void timeSigAt(int bar, int& top, int& bottom) const;

	// Time conversion — integrates over BPM/time-sig segments
	double barToSeconds(float bar) const;
	float  secondsToBar(double secs) const;
	void   secondsToBarBeat(double secs, int& bar, int& beat) const;

	// Track management
	int  addTrack(std::string label = "", int patternId = 0);
	void removeTrack(int trackId);
	void renameTrack(int trackId, std::string newLabel);
	void selectTrack(int index);

	// Pattern definition management
	int createPattern(float lengthBeats);
	int createDrumPattern(float lengthBeats);
	int createPianorollPattern(float lengthBeats);
	const Pattern*         patternForInstance(int instanceId) const;
	const PatternInstance* instanceById(int instanceId) const;

	// Pattern instance management (instances identified by stable id)
	void addPattern(int trackIndex, float startBar, float length, float patternBeats = 0.0f);
	void removePattern(int instanceId);
	void movePattern(int instanceId, int newTrackIndex, float newStartBar);
	void resizePattern(int instanceId, float newLength);
	void resizePatternLeft(int instanceId, float newStartBar, float newLength, float newStartOffset);
	void setPatternStartOffset(int instanceId, float startOffset);

	// Drum note CRUD
	void addDrumNote(int patternId, int note, float beat, float velocity = 0.8f);
	void removeDrumNote(int drumNoteId);
	std::vector<DrumNote> buildDrumPatternNotes(int patternId) const;

	// Pattern note CRUD
	void remapPatternNotes(int patId, int oldSize, int newSize);
	void addNote(int patternId, float start, int pitch, float length, float velocity = 0.8f);
	void removeNote(int noteId);
	void moveNote(int noteId, float newStart, float newPitch);
	void resizeNoteRight(int noteId, float newLength);
	void resizeNoteLeft(int noteId, float newStart, float newLength);
	std::vector<Note> buildPatternNotes(int patternId) const;

	// Default channel name assigned to newly created patterns.
	std::string defaultOutputChannel;

	// Set the output channel assignment for a pattern (empty = unassigned).
	void setPatternOutputChannel(int patId, const std::string& channelName);

	// Set the per-pattern time signature (also adjusts lengthBeats to keep bar count constant).
	void setPatternTimeSig(int patId, int top, int bottom);

	// Set the pattern definition length in beats.
	void setPatternLength(int patId, float lengthBeats);
	void renamePatternOutputChannel(const std::string& oldName, const std::string& newName);

	// Place a PatternInstance referencing an existing Pattern (no new Pattern created)
	void placePattern(int trackIndex, int patternId, float startBar, float length);

	// Build a flat Note list for grid consumption (row = track index)
	std::vector<Note> buildNotes() const;

	// Replace the entire timeline at once and notify observers.
	// Updates nextId so new IDs won't collide with existing ones.
	void loadTimeline(const Timeline& tl);

	// Copy a pattern definition (notes + length); returns new pattern id.
	// Does NOT notify — caller must add a track (which notifies) or call notify manually.
	int copyPattern(int srcPatId);

	// Remove a track and its pattern definition (if the pattern is not shared).
	void removeTrackAndPattern(int trackId);

private:
	struct TimeSegment {
		int   bar;
		float bpm;
		int   beatsPerBar;
	};
	std::vector<TimeSegment> buildSegments() const;
	Timeline data;
	std::vector<ITimelineObserver*> observers;
	int nextId = 1;

	void notify();
	void sortBpms();
	void sortTimeSigs();
};

// Swap helper: unregisters self from old timeline, registers with new one.
inline void swapObserver(ObservableTimeline*& stored, ObservableTimeline* next,
                         ITimelineObserver* self)
{
    if (stored) stored->removeObserver(self);
    stored = next;
    if (stored) stored->addObserver(self);
}

#endif
