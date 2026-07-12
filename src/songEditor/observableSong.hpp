#ifndef OBSERVABLE_SONG_HPP
#define OBSERVABLE_SONG_HPP

#include "itimelineobserver.hpp"
#include "timeline.hpp"
#include "patternNames.hpp"
#include <string>
#include <vector>

class ObservablePattern;
class ObservableInstrument;
class ITransport;

class ObservableSong {
    friend class ObservablePattern;
    friend class ObservableInstrument;
public:
    ObservableSong(float initBpm, int initTop, int initBottom);

    void addObserver(ITimelineObserver* o);
    void removeObserver(ITimelineObserver* o);

    const Timeline& get() const { return data; }

    // Optional transport, used only to keep the playhead musically anchored
    // across tempo edits (see reanchoringTempo). No-op until set.
    void setTransport(ITransport* t) { transport = t; }

    // Tempo, in beats per minute — a beat being the time signature's BeatUnit.
    void  setBpm(int bar, float bpm);
    void  removeBpm(int bar);
    void  moveBpmMarker(int fromBar, int toBar);
    float bpmAt(int bar) const;

    // Tempo in crotchets per minute: the BPM scaled by the beat definition. This
    // is the tempo the timing math and the JACK/LV2 hosts speak in.
    float cpmAt(int bar) const;

    // Time signature (+ the beat definition stored with it)
    void setTimeSig(int bar, int top, int bottom,
                    timeSettings::BeatUnit beat = timeSettings::beatUnitDefault);
    void removeTimeSig(int bar);
    void moveTimeSigMarker(int fromBar, int toBar);
    void timeSigAt(int bar, int& top, int& bottom) const;
    timeSettings::BeatUnit beatAt(int bar) const;
    // Seconds one bar lasts at `bar`, from its time signature, beat and tempo.
    double secondsPerBarAt(int bar) const;

    // How many of a pattern's grid beats fit in one song bar at `bar`. Every
    // conversion between a pattern's beats and song bars must go through this:
    // a pattern's columns are units of ITS time signature, timed by ITS beat
    // definition, so they are not the song's beats unless the two signatures
    // agree. The patternId overload falls back to the song's numerator when the
    // pattern is gone. See timeSettings::patternBeatsPerBar.
    float patternBeatsPerBar(int bar, const Pattern& pat) const;
    float patternBeatsPerBar(int bar, int patternId) const;
    const Pattern* patternById(int patternId) const;

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
    int  cloneLane(int trackId, int laneId);
    void removeLane(int trackId, int laneId);
    void setStackedLanes(int trackId, bool stacked);
    int  trackIndexForId(int trackId) const;
    int  trackIndexForLaneId(int laneId) const;
    int  trackIdForLaneId(int laneId) const;
    int  instrumentIdForTrack(int trackId) const;

    // Which context-menu items apply to a given track (greyed-out state).
    struct TrackMenuFlags {
        bool canOpenPattern = false;
        bool canRemoveLane  = false;
        bool isDrumTrack    = false;
    };
    TrackMenuFlags trackMenuFlags(int trackId) const;
    // rowOrder insertion index for a new param lane belonging to this track.
    int  paramLaneInsertIndex(int trackId) const;
    void moveRow(int fromRowIdx, int toGapIdx);
    void moveTrack(int trackId, int insertBeforeTrackId);
    // Loop editor instrument ordering (independent of the song editor's rowOrder).
    // loopOrder holds track IDs; insertBeforeTrackId < 0 appends at the end.
    void moveLoopInstrument(int trackId, int insertBeforeTrackId);
    // Loop editor pattern (lane) ordering, independent of rowOrder per track.
    // True when laneId's pattern drum-ness matches destTrackId's instrument.
    bool canMoveLaneToTrack(int laneId, int destTrackId) const;
    // Reorder within an instrument (destTrackId == lane's track) or move to another;
    // beforeLaneId < 0 appends. Returns false if the move is forbidden/not possible.
    bool moveLoopPattern(int laneId, int destTrackId, int beforeLaneId);
    void rebuildInstrumentHeaders();
    // Predict which track a lane/param row at `from` would belong to if dropped at
    // `toGap`, without mutating. Returns -1 if it cannot be determined. Mirrors the
    // destination resolution inside moveRow so callers can validate a drop first.
    int  predictRowDropTrack(int from, int toGap) const;

    // Pattern lifecycle helpers (not note editing)
    int nextTrackNumberForType(PatternType type) const;
    void removeTrackAndPattern(int trackId);
    const Pattern*         patternForInstance(int instanceId) const;
    const PatternInstance* instanceById(int instanceId) const;
    int                    laneIdForInstance(int instanceId) const;

    // Default instrument IDs assigned to newly created patterns (0 = none).
    int defaultInstrumentId     = 0;
    int defaultDrumInstrumentId = 0;

    // Set the display name of a pattern. Rejected (no change) if the name is
    // blank or duplicates another pattern; all naming is funnelled through
    // PatternNames to keep names unique.
    void setPatternName(int patId, std::string name);

    // Would renaming pattern patId to `name` collide with another pattern?
    // Used by the editors for live duplicate feedback while typing.
    bool patternNameCollides(int patId, const std::string& name) const {
        return patternNames.collides(name, patId);
    }

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
    // A stretch of the timeline with one tempo and one time signature. Held in
    // crotchets so a bar's duration is barCrotchets * 60 / cpm regardless of how
    // the beat is defined.
    struct TimeSegment {
        int    bar;
        float  cpm;           // crotchets per minute
        int    beatsPerBar;   // time-signature numerator = grid columns per bar
        double barCrotchets;
    };
    std::vector<TimeSegment> buildSegments() const;
    Timeline data;
    // Funnel for all pattern-name creation/changes; binds to data.patterns.
    PatternNames patternNames{data};
    std::vector<ITimelineObserver*> observers;
    ITransport* transport = nullptr;
    int nextId = 1;

    // Run a tempo-map mutation while keeping the transport anchored to its
    // current musical position. The transport stores its play anchor in seconds
    // (derived from the old tempo map), so any change to that map would remap
    // the same elapsed wall-clock time to a different bar and the playhead would
    // scrub. We sample the position under the old map, apply the change, then
    // re-seek so the playhead stays put and merely advances at the new rate.
    template <class F>
    void reanchoringTempo(F&& mutate);

    // True if patId is referenced by any lane's editor pattern or any placed instance.
    bool patternStillReferenced(int patId) const;

    void notify();
    // Drop stale track IDs from loopOrder and append any missing tracks.
    void reconcileLoopOrder();
    // Per track: drop stale lane IDs from loopLanes and append any missing lanes.
    void reconcileLoopLanes();
    void sortBpms();
    void sortTimeSigs();
    void removeParamLanesForInstrument(int instrumentId);
};

#endif
