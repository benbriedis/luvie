#ifndef PATTERN_EDITOR_HPP
#define PATTERN_EDITOR_HPP

#include "editor.hpp"
#include "patternGrid.hpp"
#include "noteLabels.hpp"
#include "popup.hpp"
#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <vector>

class PatternEditor : public Editor, public ITimelineObserver {
    static constexpr int labelsW = 36;

    NoteLabels          noteLabels;
    PatternGrid         patternGrid;
    ObservableTimeline* timeline          = nullptr;
    int                 lastSelectedTrack = -1;

public:
    PatternEditor(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                  int rowHeight, int colWidth, float snap, Popup& popup);
    ~PatternEditor();

    void setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex);
    void setNoteParams(int octave, int rootPitch, int chordType, bool useSharp);
    int  numPatternBeats() const { return patternGrid.numCols; }
    void onTimelineChanged() override;
};

#endif
