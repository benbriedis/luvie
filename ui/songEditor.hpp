#ifndef SONG_EDITOR_HPP
#define SONG_EDITOR_HPP

#include "editor.hpp"
#include "songGrid.hpp"
#include "trackLabels.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <vector>

class SongEditor : public Editor {
    static constexpr int labelW = 80;

    TrackLabels trackLabels;
    SongGrid    songGrid;

public:
    SongEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
               int rowHeight, int colWidth, float snap, Popup& popup);

    std::function<void(int trackIndex)> onPatternDoubleClick;

    void setTransport(ITransport* t, ObservableTimeline* tl);
    void setTrackView(int trackIndex, bool beatResolution);
};

#endif
