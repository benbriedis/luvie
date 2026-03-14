#include "trackLabels.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

// While a track label is being edited, intercept FL_PUSH events before they
// reach any widget.  Fl::event_dispatch() is called ahead of all widget dispatch.
static TrackLabels*      activeEditor     = nullptr;
static Fl_Event_Dispatch originalDispatch = nullptr;
static bool              commitPending    = false;  // true between consuming push and release

static int editDispatch(int event, Fl_Window* window)
{
    // Phase 2: we already consumed a push outside the labels — also consume
    // drag and release so they never reach the grid or rulers.
    if (commitPending) {
        if (event == FL_DRAG) return 1;
        if (event == FL_RELEASE) {
            commitPending = false;
            if (activeEditor) activeEditor->commitEdit();
            return 1;
        }
    }

    // Phase 1: watch for a click outside the labels panel.
    if (activeEditor && event == FL_PUSH) {
        int ex = Fl::event_x(), ey = Fl::event_y();
        bool inLabels = ex >= activeEditor->x() && ex < activeEditor->x() + activeEditor->w()
                     && ey >= activeEditor->y() && ey < activeEditor->y() + activeEditor->h();
        if (!inLabels) {
            commitPending = true;
            return 1;  // consume push; wait for release before committing
        }
    }

    return Fl::handle_(event, window);
}

static constexpr Fl_Color colNormal   = 0x1F293700;
static constexpr Fl_Color colSelected = 0x3B82F600;
static constexpr Fl_Color colText     = FL_WHITE;
static constexpr Fl_Color colBorder   = 0x37415100;

TrackLabels::TrackLabels(int x, int y, int w, int rowHeight)
    : Fl_Group(x, y, w, 0),
      rowHeight(rowHeight),
      input(x, y, w, rowHeight)
{
    input.hide();
    input.box(FL_FLAT_BOX);
    input.color(colSelected);
    input.textcolor(colText);
    input.cursor_color(colText);
    input.when(FL_WHEN_ENTER_KEY);
    input.callback([](Fl_Widget*, void* data) {
        static_cast<TrackLabels*>(data)->commitEdit();
    }, this);
    input.onUnfocus([this]() { commitEdit(); });
    end();
}

TrackLabels::~TrackLabels()
{
    swapObserver(timeline, nullptr, this);
}

void TrackLabels::setTimeline(ObservableTimeline* tl)
{
    swapObserver(timeline, tl, this);
    if (timeline) {
        int rows = (int)timeline->get().tracks.size();
        size(w(), rows * rowHeight);
    }
    redraw();
}

void TrackLabels::startEdit(int trackIndex)
{
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    if (trackIndex < 0 || trackIndex >= (int)tracks.size()) return;
    editingTrackIndex = trackIndex;
    int ry = y() + trackIndex * rowHeight;
    input.resize(x(), ry, w(), rowHeight);
    input.value(tracks[trackIndex].label.c_str());
    input.show();
    input.take_focus();
    activeEditor     = this;
    originalDispatch = Fl::event_dispatch();
    Fl::event_dispatch(editDispatch);
    redraw();
}

void TrackLabels::commitEdit()
{
    if (editingTrackIndex < 0) return;
    int idx = editingTrackIndex;
    editingTrackIndex = -1;  // clear first to guard against re-entry via FL_UNFOCUS on hide
    Fl::event_dispatch(originalDispatch);
    activeEditor     = nullptr;
    originalDispatch = nullptr;
    commitPending    = false;
    std::string newLabel = input.value();
    input.hide();
    if (timeline) {
        const auto& tracks = timeline->get().tracks;
        if (idx < (int)tracks.size())
            timeline->renameTrack(tracks[idx].id, newLabel);
    }
    redraw();
}

void TrackLabels::cancelEdit()
{
    if (editingTrackIndex < 0) return;
    editingTrackIndex = -1;
    Fl::event_dispatch(originalDispatch);
    activeEditor     = nullptr;
    originalDispatch = nullptr;
    commitPending    = false;
    input.hide();
    redraw();
}

void TrackLabels::draw()
{
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    int sel = timeline->get().selectedTrackIndex;

    fl_font(FL_HELVETICA, 11);
    for (int i = 0; i < (int)tracks.size(); i++) {
        if (i == editingTrackIndex) continue;  // input widget draws itself
        int ry = y() + i * rowHeight;
        fl_color(i == sel ? colSelected : colNormal);
        fl_rectf(x(), ry, w(), rowHeight);
        fl_color(colBorder);
        fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
        fl_color(colText);
        fl_draw(tracks[i].label.c_str(), x() + 4, ry, w() - 8, rowHeight, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
    }
    draw_children();
}

int TrackLabels::handle(int event)
{
    if (event == FL_ENTER) {
        window()->cursor(FL_CURSOR_DEFAULT);
        return 1;
    }

    // Let child widgets (the input) handle events first
    if (Fl_Group::handle(event))
        return 1;

    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
        if (!timeline) return 1;
        int row = (Fl::event_y() - y()) / rowHeight;

        if (editingTrackIndex >= 0 && row != editingTrackIndex)
            commitEdit();

        if (row >= 0 && row < (int)timeline->get().tracks.size()) {
            if (Fl::event_clicks() == 1)
                startEdit(row);
            else
                timeline->selectTrack(row);
        }
        return 1;
    }

    if (event == FL_KEYDOWN && editingTrackIndex >= 0) {
        if (Fl::event_key() == FL_Escape) { cancelEdit(); return 1; }
    }

    return 0;
}
