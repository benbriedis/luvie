#include "trackLabels.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>

static constexpr Fl_Color colNormal   = 0x1F293700;  // dark slate
static constexpr Fl_Color colSelected = 0x3B82F600;  // blue
static constexpr Fl_Color colText     = FL_WHITE;
static constexpr Fl_Color colBorder   = 0x37415100;  // slightly lighter than bg

TrackLabels::TrackLabels(int x, int y, int w, int rowHeight)
    : Fl_Widget(x, y, w, 0), rowHeight(rowHeight)
{}

TrackLabels::~TrackLabels()
{
    if (timeline) timeline->removeObserver(this);
}

void TrackLabels::setTimeline(ObservableTimeline* tl)
{
    if (timeline) timeline->removeObserver(this);
    timeline = tl;
    if (timeline) {
        int rows = (int)timeline->get().tracks.size();
        size(w(), rows * rowHeight);
        timeline->addObserver(this);
    }
    redraw();
}

void TrackLabels::draw()
{
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    int sel = timeline->get().selectedTrackIndex;

    fl_font(FL_HELVETICA, 11);
    for (int i = 0; i < (int)tracks.size(); i++) {
        int ry = y() + i * rowHeight;
        fl_color(i == sel ? colSelected : colNormal);
        fl_rectf(x(), ry, w(), rowHeight);
        fl_color(colBorder);
        fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
        fl_color(colText);
        fl_draw(tracks[i].label.c_str(), x() + 4, ry, w() - 8, rowHeight, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
    }
}

int TrackLabels::handle(int event)
{
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
        if (!timeline) return 1;
        int row = (Fl::event_y() - y()) / rowHeight;
        if (row >= 0 && row < (int)timeline->get().tracks.size())
            timeline->selectTrack(row);
        return 1;
    }
    return Fl_Widget::handle(event);
}
