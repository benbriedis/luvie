#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"

class SongGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    int                 trackFilter       = -1;
    bool                beatResolution    = false;
    int                 draggingPatternId = -1;
    float               originalLength    = 1.0f;
    bool                isDragging        = false;
    float               tickBarPos        = 0.0f;  // song-bar position of beat-0 tick, captured at drag start
    int                 dragBeatsPerBar   = 4;

    void rebuildNotes();

protected:
    void draw() override;
    void resizing() override;
    std::function<void()> makeDeleteCallback() override;
    void onBeginDrag() override;
    void onCommitDrag() override;
    void toggleNote() override;

public:
    SongGrid(std::vector<Note> notes, int numRows, int numCols,
             int rowHeight, int colWidth, float snap, Popup& popup);
    ~SongGrid();

    void setTimeline(ObservableTimeline* tl);
    void setTrackView(int trackFilter, bool beatResolution);
    void onTimelineChanged() override;
};

#endif
