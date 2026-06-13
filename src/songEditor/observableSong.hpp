#ifndef OBSERVABLE_SONG_HPP
#define OBSERVABLE_SONG_HPP

#include "itimelineobserver.hpp"
#include "timeline.hpp"
#include <string>
#include <vector>

class ObservablePattern;
class ObservableInstrument;

class ObservableSong {
    friend class ObservablePattern;
    friend class ObservableInstrument;
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

    // Instrument management
    int  addInstrument(std::string name, bool isDrum = false);
    void renameInstrument(int instrId, std::string name);
    void removeInstrument(int instrId);

    // Track management
    int  addTrack(int instrumentId = 0, int patternId = 0, int atIndex = -1);
    void removeTrack(int trackId);
    void setTrackSolo(int trackId, bool solo);
    void setTrackMute(int trackId, bool mute);
    bool isTrackPlaying(int trackId) const;
    void selectTrack(int index);
    void selectLane(int trackIndex, int laneId);
    int  addLane(int trackId);
    int  addPianorollLane(int trackId);
    void removeLane(int trackId, int laneId);
    void setStackedLanes(int trackId, bool stacked);
    int  trackIndexForId(int trackId) const;
    int  trackIndexForLaneId(int laneId) const;
    int  trackIdForLaneId(int laneId) const;
    void moveRow(int fromRowIdx, int toGapIdx);
    void moveTrack(int trackId, int insertBeforeTrackId);
    void rebuildInstrumentHeaders();

    // Pattern lifecycle helpers (not note editing)
    int nextTrackNumberForType(PatternType type) const;
    void removeTrackAndPattern(int trackId);
    const Pattern*         patternForInstance(int instanceId) const;
    const PatternInstance* instanceById(int instanceId) const;
    int                    laneIdForInstance(int instanceId) const;

    // Default instrument IDs assigned to newly created patterns (0 = none).
    int defaultInstrumentId     = 0;
    int defaultDrumInstrumentId = 0;

    // Set the display name of a pattern; empty = auto ("InstrumentName N").
    void setPatternName(int patId, std::string name);

    // Pattern instance management (instances identified by stable id)
    void addPattern(int trackIndex, float startBar, float length, float patternBeats = 0.0f);
    void removePattern(int instanceId);
    void movePattern(int instanceId, int newLaneId, float newStartBar);
    void resizePattern(int instanceId, float newLength);
    void resizePatternLeft(int instanceId, float newStartBar, float newLength, float newStartOffset);
    void setPatternStartOffset(int instanceId, float startOffset);
    void placePattern(int laneId, int patternId, float startBar, float length);

    // Song-level param lane management. Each lane belongs to one instrument and
    // routes only to that instrument's port; uniqueness is per (type, instrument).
    bool hasParamLane(const std::string& type, int instrumentId) const;
    int  addParamLane(const std::string& type, int instrumentId, int atIndex = -1);
    void removeParamLane(int laneId);
    int  instrumentIdForParamLane(int laneId) const;
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
    int nextPatternNumber = 1;

    void notify();
    void sortBpms();
    void sortTimeSigs();
    void removeParamLanesForInstrument(int instrumentId);
};

#endif
