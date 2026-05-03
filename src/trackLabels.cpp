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

void TrackLabels::setTimeline(ObservableSong* tl)
{
    swapObserver(timeline, tl, this);
    redraw();
}

void TrackLabels::setRowOffset(int offset)
{
    rowOffset = offset;
    redraw();
}

void TrackLabels::startEdit(int absRow)
{
    if (!timeline) return;
    const auto& ro = timeline->get().rowOrder;
    if (absRow < 0 || absRow >= (int)ro.size() || !ro[absRow].isTrack) return;
    int trackId = ro[absRow].id;
    for (const auto& t : timeline->get().tracks) {
        if (t.id != trackId) continue;
        editingAbsRow = absRow;
        originalLabel = t.label;
        int ry = y() + (absRow - rowOffset) * rowHeight;
        input.resize(x(), ry, w(), rowHeight);
        input.value(originalLabel.c_str());
        input.textcolor(colText);
        input.show();
        input.take_focus();
        input.position(input.size(), 0);  // select all
        input.onChange([this]() { checkDuplicate(); });
        InlineEditDispatch::install(this, [this]() { commitEdit(); });
        redraw();
        return;
    }
}

void TrackLabels::checkDuplicate()
{
    if (!timeline || editingAbsRow < 0) return;
    const auto& ro = timeline->get().rowOrder;
    if (editingAbsRow >= (int)ro.size() || !ro[editingAbsRow].isTrack) return;
    int editingId   = ro[editingAbsRow].id;
    std::string cur = input.value();
    bool dup = false;
    for (const auto& t : timeline->get().tracks)
        if (t.id != editingId && t.label == cur) { dup = true; break; }
    input.textcolor(dup ? FL_RED : colText);
    input.redraw();
}

void TrackLabels::commitEdit()
{
    if (editingAbsRow < 0) return;
    int absRow    = editingAbsRow;
    editingAbsRow = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = input.value();
    input.hide();
    if (timeline) {
        const auto& ro = timeline->get().rowOrder;
        if (absRow < (int)ro.size() && ro[absRow].isTrack) {
            int trackId = ro[absRow].id;
            bool dup = false;
            for (const auto& t : timeline->get().tracks)
                if (t.id != trackId && t.label == newLabel) { dup = true; break; }
            timeline->renameTrack(trackId, dup ? originalLabel : newLabel);
        }
    }
    redraw();
}

void TrackLabels::cancelEdit()
{
    if (editingAbsRow < 0) return;
    editingAbsRow = -1;
    InlineEditDispatch::uninstall();
    input.hide();
    redraw();
}

void TrackLabels::draw()
{
    if (!timeline) { draw_children(); return; }

    const auto& tl  = timeline->get();
    const auto& ro  = tl.rowOrder;
    int         sel = tl.selectedTrackIndex;
    int selectedId  = (sel >= 0 && sel < (int)tl.tracks.size()) ? tl.tracks[sel].id : -1;

    fl_font(FL_HELVETICA, 11);
    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        if (i == editingAbsRow) continue;  // input widget draws itself
        int ry = y() + (i - rowOffset) * rowHeight;

        if (i >= 0 && i < (int)ro.size()) {
            const auto& ref = ro[i];
            bool isDragSrc = (dragging && i == dragRow);
            if (ref.isTrack) {
                bool isSel = (ref.id == selectedId);
                Fl_Color bg = isSel ? colSelected : colNormal;
                if (isDragSrc) bg = fl_color_average(bg, FL_WHITE, 0.75f);
                fl_color(bg);
                fl_rectf(x(), ry, w(), rowHeight);
                fl_color(colBorder);
                fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
                fl_color(isDragSrc ? fl_color_average(colText, FL_WHITE, 0.5f) : colText);
                for (const auto& t : tl.tracks)
                    if (t.id == ref.id) {
                        fl_draw(t.label.c_str(), x() + 4, ry, w() - 8, rowHeight,
                                FL_ALIGN_LEFT | FL_ALIGN_CLIP);
                        break;
                    }
            } else {
                Fl_Color bg = isDragSrc ? fl_color_average(colParam, FL_WHITE, 0.75f) : colParam;
                fl_color(bg);
                fl_rectf(x(), ry, w(), rowHeight);
                fl_color(colBorder);
                fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
                fl_color(isDragSrc ? fl_color_average(colText, FL_WHITE, 0.5f) : colText);
                for (const auto& lane : tl.paramLanes)
                    if (lane.id == ref.id) {
                        fl_draw(lane.type.c_str(), x() + 4, ry, w() - 8, rowHeight,
                                FL_ALIGN_LEFT | FL_ALIGN_CLIP);
                        break;
                    }
            }
        } else {
            fl_color(colEmpty);
            fl_rectf(x(), ry, w(), rowHeight);
            fl_color(colBorder);
            fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);
        }
    }

    // Drop indicator line
    if (dragging && dropRow >= 0) {
        int lineVR = dropRow - rowOffset;
        int lineY  = std::clamp(y() + lineVR * rowHeight, y(), y() + numVisibleRows * rowHeight);
        fl_color(0x00BFFFFF);  // cyan
        fl_line_style(FL_SOLID, 2);
        fl_line(x(), lineY, x() + w() - 1, lineY);
        fl_line_style(0);
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
        const auto& ro = timeline->get().rowOrder;

        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (row >= 0 && row < (int)ro.size()) {
                const auto& ref = ro[row];
                if (ref.isTrack) {
                    if (contextPopup && patternObs)
                        contextPopup->open(ref.id, patternObs,
                                           Fl::event_x_root(), Fl::event_y_root());
                } else {
                    if (paramLanePopup && patternObs)
                        paramLanePopup->open(ref.id, patternObs,
                                             Fl::event_x_root(), Fl::event_y_root());
                }
            }
            return 1;
        }

        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (editingAbsRow >= 0 && row != editingAbsRow)
                commitEdit();

            if (Fl::event_clicks() == 1) {
                // Double-click: start edit immediately, no drag
                startEdit(row);
                dragStartRow = -1;
                return 1;
            }

            // Single click: record potential drag start; commit on release
            dragStartRow = row;
            dragStartY   = Fl::event_y();
            dragging     = false;
            dragRow      = -1;
            dropRow      = -1;
            return 1;
        }
    }

    if (event == FL_DRAG) {
        if (dragStartRow < 0) return 0;
        if (!dragging && std::abs(Fl::event_y() - dragStartY) > dragThreshold) {
            dragging = true;
            dragRow  = dragStartRow;
        }
        if (dragging && timeline) {
            const auto& ro = timeline->get().rowOrder;
            int mouseRelY = Fl::event_y() - y();
            int gap = (mouseRelY + rowHeight / 2) / rowHeight + rowOffset;
            dropRow = std::clamp(gap, 0, (int)ro.size());
            redraw();
        }
        return 1;
    }

    if (event == FL_RELEASE) {
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (dragging) {
                if (timeline && dragRow >= 0 && dropRow >= 0)
                    timeline->moveRow(dragRow, dropRow);
                dragging = false;
                dragRow  = -1;
                dropRow  = -1;
            } else if (dragStartRow >= 0 && timeline) {
                // Pure single click: select track
                const auto& ro = timeline->get().rowOrder;
                if (dragStartRow < (int)ro.size() && ro[dragStartRow].isTrack) {
                    int trackIdx = timeline->trackIndexForId(ro[dragStartRow].id);
                    if (trackIdx >= 0) timeline->selectTrack(trackIdx);
                }
            }
            dragStartRow = -1;
            return 1;
        }
    }

    if (event == FL_KEYDOWN && editingAbsRow >= 0) {
        if (Fl::event_key() == FL_Escape) { cancelEdit(); return 1; }
    }

    return 0;
}
