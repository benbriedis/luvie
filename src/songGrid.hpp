#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"

class SongGrid : public Grid, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    int                 trackFilter       = -1;
    bool                beatResolution    = false;
    float               tickBarPos        = 0.0f;
    float               dragStartOffset   = 0.0f;
    int                 dragBeatsPerBar   = 4;
    int                 rowOffset         = 0;

    void rebuildNotes();

protected:
    void draw() override;
    void resizing(StateDragResize& s) override;
    std::function<void()> makeDeleteCallback(int noteIdx) override;
    void onBeginDrag(int noteIdx) override;
    void onCommitMove(const StateDragMove& s) override;
    void onCommitResize(const StateDragResize& s) override;
    void onNoteDoubleClick(int noteIdx) override;
    void toggleNote() override;

public:
    SongGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup);
    ~SongGrid();

    std::function<void(int trackIndex)> onPatternDoubleClick;

    void setTimeline(ObservableTimeline* tl);
    void setTrackView(int trackFilter, bool beatResolution);
    void setRowOffset(int offset);
    void onTimelineChanged() override;
};

#endif
