#include "trackLabels.hpp"
#include "trackContextPopup.hpp"
#include "paramLaneContextPopup.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

static constexpr Fl_Color colNormal   = 0x1F293700;
static constexpr Fl_Color colSelected = 0x3B82F600;
static constexpr Fl_Color colEmpty    = 0x17202A00;
static constexpr Fl_Color colParam    = 0x1A2B3A00;  // param lane label bg
static constexpr Fl_Color colText     = FL_WHITE;
static constexpr Fl_Color colBorder   = 0x37415100;

TrackLabels::TrackLabels(int x, int y, int w, int numVisibleRows, int rowHeight)
    : Fl_Group(x, y, w, numVisibleRows * rowHeight),
      numVisibleRows(numVisibleRows),
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
    redraw();
}

void TrackLabels::setRowOffset(int offset)
{
    rowOffset = offset;
    redraw();
}

void TrackLabels::startEdit(int trackIndex)
{
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    if (trackIndex < 0 || trackIndex >= (int)tracks.size()) return;
    editingTrackIndex = trackIndex;
    originalLabel     = tracks[trackIndex].label;
    int ry = y() + (trackIndex - rowOffset) * rowHeight;
    input.resize(x(), ry, w(), rowHeight);
    input.value(originalLabel.c_str());
    input.textcolor(colText);
    input.show();
    input.take_focus();
    input.position(input.size(), 0);  // select all
    input.onChange([this]() { checkDuplicate(); });
    InlineEditDispatch::install(this, [this]() { commitEdit(); });
    redraw();
}

void TrackLabels::checkDuplicate()
{
    if (!timeline) return;
    const auto& tracks = timeline->get().tracks;
    std::string current = input.value();
    bool dup = false;
    for (int i = 0; i < (int)tracks.size(); i++) {
        if (i != editingTrackIndex && tracks[i].label == current) { dup = true; break; }
    }
    input.textcolor(dup ? FL_RED : colText);
    input.redraw();
}

void TrackLabels::commitEdit()
{
    if (editingTrackIndex < 0) return;
    int idx = editingTrackIndex;
    editingTrackIndex = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = input.value();
    input.hide();
    if (timeline) {
        const auto& tracks = timeline->get().tracks;
        if (idx < (int)tracks.size()) {
            bool dup = false;
            for (int i = 0; i < (int)tracks.size(); i++) {
                if (i != idx && tracks[i].label == newLabel) { dup = true; break; }
            }
            timeline->renameTrack(tracks[idx].id, dup ? originalLabel : newLabel);
        }
    }
    redraw();
}

void TrackLabels::cancelEdit()
{
    if (editingTrackIndex < 0) return;
    editingTrackIndex = -1;
    InlineEditDispatch::uninstall();
    input.hide();
    redraw();
}

void TrackLabels::draw()
{
    int totalTracks = timeline ? (int)timeline->get().tracks.size() : 0;
    int totalParams = timeline ? (int)timeline->get().paramLanes.size() : 0;
    int sel         = timeline ? timeline->get().selectedTrackIndex : -1;

    fl_font(FL_HELVETICA, 11);
    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        if (i == editingTrackIndex) continue;  // input widget draws itself
        int ry = y() + (i - rowOffset) * rowHeight;

        if (i < totalTracks) {
            fl_color(i == sel ? colSelected : colNormal);
            fl_rectf(x(), ry, w(), rowHeight);
            fl_color(colBorder);
            fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
            fl_color(colText);
            fl_draw(timeline->get().tracks[i].label.c_str(),
                    x() + 4, ry, w() - 8, rowHeight,
                    FL_ALIGN_LEFT | FL_ALIGN_CLIP);
        } else {
            int laneIdx = i - totalTracks;
            if (laneIdx >= 0 && laneIdx < totalParams) {
                // Param lane label
                fl_color(colParam);
                fl_rectf(x(), ry, w(), rowHeight);
                fl_color(colBorder);
                fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
                fl_color(colText);
                fl_draw(timeline->get().paramLanes[laneIdx].type.c_str(),
                        x() + 4, ry, w() - 8, rowHeight,
                        FL_ALIGN_LEFT | FL_ALIGN_CLIP);
            } else {
                // Empty row below all tracks and param lanes
                fl_color(colEmpty);
                fl_rectf(x(), ry, w(), rowHeight);
                fl_color(colBorder);
                fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
            }
        }
    }
    draw_children();
}

int TrackLabels::handle(int event)
{
    if (event == FL_ENTER) {
        window()->cursor(FL_CURSOR_DEFAULT);
        return 1;
    }

    if (Fl_Group::handle(event))
        return 1;

    if (event == FL_PUSH) {
        if (!timeline) return 1;
        int row = (Fl::event_y() - y()) / rowHeight + rowOffset;
        int numTracks = (int)timeline->get().tracks.size();

        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (row >= 0 && row < numTracks) {
                if (contextPopup)
                    contextPopup->open(row, timeline,
                                       Fl::event_x_root(), Fl::event_y_root());
            } else {
                int laneIdx = row - numTracks;
                int numParams = (int)timeline->get().paramLanes.size();
                if (paramLanePopup && laneIdx >= 0 && laneIdx < numParams) {
                    int laneId = timeline->get().paramLanes[laneIdx].id;
                    paramLanePopup->open(laneId, timeline,
                                        Fl::event_x_root(), Fl::event_y_root());
                }
            }
            return 1;
        }

        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (editingTrackIndex >= 0 && row != editingTrackIndex)
                commitEdit();

            if (row >= 0 && row < numTracks) {
                if (Fl::event_clicks() == 1)
                    startEdit(row);
                else
                    timeline->selectTrack(row);
            }
            return 1;
        }
    }

    if (event == FL_KEYDOWN && editingTrackIndex >= 0) {
        if (Fl::event_key() == FL_Escape) { cancelEdit(); return 1; }
    }

    return 0;
}
