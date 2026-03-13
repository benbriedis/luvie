#ifndef TRACK_LABELS_HPP
#define TRACK_LABELS_HPP

#include "observableTimeline.hpp"
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <functional>

// Fl_Input subclass that fires a callback when focus leaves it.
class InlineInput : public Fl_Input {
    std::function<void()> unfocusCb;
public:
    InlineInput(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {}
    void onUnfocus(std::function<void()> cb) { unfocusCb = std::move(cb); }
    int handle(int event) override {
        int r = Fl_Input::handle(event);
        if (event == FL_UNFOCUS && unfocusCb) unfocusCb();
        return r;
    }
};

class TrackLabels : public Fl_Group, public ITimelineObserver {
    ObservableTimeline* timeline          = nullptr;
    int                 rowHeight;
    InlineInput         input;
    int                 editingTrackIndex = -1;

    void startEdit(int trackIndex);
    void cancelEdit();

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
