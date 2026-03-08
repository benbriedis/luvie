#ifndef SONG_GRID_HPP
#define SONG_GRID_HPP

#include "grid.hpp"
#include "observableTimeline.hpp"

class SongGrid : public MyGrid, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    int                 trackFilter       = -1;
    bool                beatResolution    = false;
    int                 draggingPatternId = -1;
    float               originalLength    = 1.0f;
    bool                isDragging        = false;

    void rebuildNotes();

protected:
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
