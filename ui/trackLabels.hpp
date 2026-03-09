#ifndef TRACK_LABELS_HPP
#define TRACK_LABELS_HPP

#include "observableTimeline.hpp"
#include <FL/Fl_Widget.H>

class TrackLabels : public Fl_Widget, public ITimelineObserver {
    ObservableTimeline* timeline  = nullptr;
    int                 rowHeight;

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
