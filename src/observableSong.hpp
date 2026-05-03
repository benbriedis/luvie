#ifndef OBSERVABLE_SONG_HPP
#define OBSERVABLE_SONG_HPP

#include "itimelineobserver.hpp"
#include "timeline.hpp"
#include <string>
#include <vector>

class ObservablePattern;

class ObservableSong {
    friend class ObservablePattern;
public:
    ObservableSong(float initBpm, int initTop, int initBottom);

    void addObserver(ITimelineObserver* o);
    void removeObserver(ITimelineObserver* o);

    const Timeline& get() const { return data; }

    // BPM
    void  setBpm(int bar, float bpm);
    void  removeBpm(int bar);
    void  moveBpmMarker(int fromBar, int toBar);
    float bpmAt(int bar) const;

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
    int  addTrack(std::string label = "", int patternId = 0, int atIndex = -1);
    void removeTrack(int trackId);
    void renameTrack(int trackId, std::string newLabel);
    void selectTrack(int index);
    int  trackIndexForId(int trackId) const;
    void moveRow(int fromRowIdx, int toGapIdx);

    // Pattern lifecycle helpers (not note editing)
    int nextTrackNumberForType(PatternType type) const;
    void removeTrackAndPattern(int trackId);
    const Pattern*         patternForInstance(int instanceId) const;
    const PatternInstance* instanceById(int instanceId) const;

    // Default instrument names assigned to newly created patterns.
    std::string defaultOutputInstrument;
    std::string defaultDrumOutputInstrument;

    // Rename all pattern output instrument assignments across the whole song.
    void renamePatternOutputInstrument(const std::string& oldName, const std::string& newName);

    // Pattern instance management (instances identified by stable id)
    void addPattern(int trackIndex, float startBar, float length, float patternBeats = 0.0f);
    void removePattern(int instanceId);
    void movePattern(int instanceId, int newTrackIndex, float newStartBar);
    void resizePattern(int instanceId, float newLength);
    void resizePatternLeft(int instanceId, float newStartBar, float newLength, float newStartOffset);
    void setPatternStartOffset(int instanceId, float startOffset);
    void placePattern(int trackIndex, int patternId, float startBar, float length);

    // Song-level param lane management
    bool hasParamLane(const std::string& type) const;
    int  addParamLane(const std::string& type, int atIndex = -1);
    void removeParamLane(int laneId);
    int  addParamPoint(int laneId, float beat, int value);
    void removeParamPoint(int pointId);
    void moveParamPoint(int pointId, float beat, int value);

    // Build a flat Note list for grid consumption (row = track index)
    std::vector<Note> buildNotes() const;

    // Replace the entire timeline at once and notify observers.
    void loadTimeline(const Timeline& tl);

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

#endif
