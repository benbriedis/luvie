#ifndef OUTER_SONG_GRID_HPP
#define OUTER_SONG_GRID_HPP

#include "outerGrid.hpp"
#include "songGrid.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <vector>

class OuterSongGrid : public OuterGrid {
    SongGrid songGrid;

public:
    OuterSongGrid(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, Popup& popup);

    void setTransport(ITransport* t, ObservableTimeline* tl);
    void setTrackView(int trackIndex, bool beatResolution);
    void setPatternBeats(float b) { songGrid.setPatternBeats(b); }
    void setDefaultPatternId(int id) { songGrid.setDefaultPatternId(id); }
};

#endif
