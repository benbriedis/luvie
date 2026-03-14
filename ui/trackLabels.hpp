#ifndef TRACK_LABELS_HPP
#define TRACK_LABELS_HPP

#include "observableTimeline.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>

class TrackLabels : public Fl_Group, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    int                 rowHeight;
    InlineInput         input;
    int                 editingTrackIndex = -1;
    std::string         originalLabel;

    void startEdit(int trackIndex);
    void cancelEdit();
    void checkDuplicate();

public:
    void commitEdit();

public:
    TrackLabels(int x, int y, int w, int rowHeight);
    ~TrackLabels();

    void setTimeline(ObservableTimeline* tl);
    void onTimelineChanged() override { redraw(); }

protected:
    void draw() override;
    int  handle(int event) override;
};

#endif
