#ifndef SONG_EDITOR_HPP
#define SONG_EDITOR_HPP

#include "editor.hpp"
#include "songGrid.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <vector>

class SongEditor : public Editor {
    SongGrid songGrid;

public:
    SongEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
               int rowHeight, int colWidth, float snap, Popup& popup);

    void setTransport(ITransport* t, ObservableTimeline* tl);
    void setTrackView(int trackIndex, bool beatResolution);
    void setPatternBeats(float b) { songGrid.setPatternBeats(b); }
    void setDefaultPatternId(int id) { songGrid.setDefaultPatternId(id); }
};

#endif
