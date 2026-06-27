#include "trackLabels.hpp"
#include "trackContextPopup.hpp"
#include "cursors.hpp"
#include "paramLaneContextPopup.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <set>
#include <vector>

static constexpr Fl_Color colNormal      = 0x1F293700;
static constexpr Fl_Color colSelected    = 0x3B82F600;
static constexpr Fl_Color colEmpty       = 0x17202A00;
static constexpr Fl_Color colParam       = 0x1A2B3A00;  // param lane label bg
static constexpr Fl_Color colText        = FL_WHITE;
static constexpr Fl_Color colBorder      = 0x37415100;
static constexpr Fl_Color colTrackDiv    = 0x64748B00;  // bright separator between tracks
static constexpr Fl_Color colInstrHeader = 0x64748B00;  // dedicated instrument name row bg
static constexpr int      instrNameRowH  = 24;           // height of instrument header rows
static constexpr int      iconAreaW      = 16;           // width reserved for expand/collapse arrow

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

int TrackLabels::rowHFor(int absRow) const
{
    if (!timeline) return rowHeight;
    const auto& ro = timeline->get().rowOrder;
    if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Header)
        return instrNameRowH;
    return rowHeight;
}

int TrackLabels::rowYInPanel(int absRow) const
{
    int py = -pixelOffset;
    for (int i = rowOffset; i < absRow; i++)
        py += rowHFor(i);
    return py;
}

int TrackLabels::absRowAtPanelY(int py) const
{
    int cumY = -pixelOffset;
    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        int rh = rowHFor(i);
        if (py < cumY + rh) return i;
        cumY += rh;
    }
    return rowOffset + numVisibleRows;
}

void TrackLabels::setScroll(int rowOff, int pxOff)
{
    rowOffset   = rowOff;
    pixelOffset = pxOff;
    redraw();
}

bool TrackLabels::trackIsDrum(int trackId) const
{
    if (!timeline || trackId < 0) return false;
    return timeline->get().instrumentIsDrum(timeline->instrumentIdForTrack(trackId));
}

// Drum and non-drum instruments aren't interchangeable, so a track may only be
// dropped where it stays adjacent to a track of its own kind. Reordering within
// the drum group or within the non-drum group is allowed; crossing the boundary
// (so the dragged track would land between two tracks of the other kind) is not.
bool TrackLabels::trackDropAllowed() const
{
    if (!timeline || dragTrackId < 0) return true;
    if (dropTrackId == dragTrackId) return true;  // dropping in place is a no-op

    // Ordered list of unique track IDs in display order.
    std::vector<int> order;
    std::set<int> seen;
    for (const auto& r : timeline->get().rowOrder) {
        int tid = -1;
        if (r.kind == RowKind::Header) tid = r.id;
        else if (r.kind == RowKind::Lane) tid = timeline->trackIdForLaneId(r.id);
        if (tid >= 0 && seen.insert(tid).second) order.push_back(tid);
    }

    // Remove the dragged track, then locate the insertion point.
    order.erase(std::remove(order.begin(), order.end(), dragTrackId), order.end());
    int pos = (int)order.size();
    if (dropTrackId >= 0)
        for (int i = 0; i < (int)order.size(); i++)
            if (order[i] == dropTrackId) { pos = i; break; }

    bool dragDrum = trackIsDrum(dragTrackId);
    int prev = pos > 0                 ? order[pos - 1] : -1;
    int next = pos < (int)order.size() ? order[pos]     : -1;
    // Only forbid wedging the track between two tracks of the opposite kind (that
    // interleaves the groups). Landing at an edge of the other group is fine.
    bool prevOpposite = prev >= 0 && trackIsDrum(prev) != dragDrum;
    bool nextOpposite = next >= 0 && trackIsDrum(next) != dragDrum;
    return !(prevOpposite && nextOpposite);
}

void TrackLabels::startInstrumentEdit(int absRow)
{
    if (!timeline) return;
    const auto& ro = timeline->get().rowOrder;
    if (absRow < 0 || absRow >= (int)ro.size()) return;
    const auto& ref = ro[absRow];

    int trackId = -1;
    if (ref.kind == RowKind::Header) {
        trackId = ref.id;
    } else if (ref.kind == RowKind::Lane) {
        int tIdx = timeline->trackIndexForLaneId(ref.id);
        if (tIdx >= 0) {
            const auto& track = timeline->get().tracks[tIdx];
            if (track.stackedLanes && !track.lanes.empty() && track.lanes[0].id == ref.id)
                trackId = track.id;
        }
    }
    if (trackId < 0) return;

    for (const auto& t : timeline->get().tracks) {
        if (t.id != trackId) continue;
        editingAbsRow = absRow;
        editingPatId  = -1;
        originalLabel = timeline->get().instrumentName(t.instrumentId);
        int ry = y() + rowYInPanel(absRow);
        input.resize(x(), ry, w(), rowHFor(absRow));
        input.value(originalLabel.c_str());
        input.color(colSelected);
        input.textcolor(colText);
        input.show();
        input.take_focus();
        input.position(input.size(), 0);
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
    if (editingAbsRow >= (int)ro.size()) return;
    const auto& ref = ro[editingAbsRow];

    int trackId = -1;
    if (ref.kind == RowKind::Header)
        trackId = ref.id;
    else if (ref.kind == RowKind::Lane)
        trackId = timeline->trackIdForLaneId(ref.id);
    if (trackId < 0) return;

    std::string cur = input.value();
    int editingInstrId = 0;
    for (const auto& t : timeline->get().tracks)
        if (t.id == trackId) { editingInstrId = t.instrumentId; break; }
    bool dup = false;
    for (const auto& instr : timeline->get().instruments)
        if (instr.id != editingInstrId && instr.name == cur) { dup = true; break; }
    input.textcolor(dup ? FL_RED : colText);
    input.redraw();
}

void TrackLabels::startPatternEdit(int absRow)
{
    if (!timeline) return;
    const auto& tl = timeline->get();
    const auto& ro = tl.rowOrder;
    if (absRow < 0 || absRow >= (int)ro.size() || ro[absRow].kind != RowKind::Lane) return;

    int laneId = ro[absRow].id;
    int tIdx   = timeline->trackIndexForLaneId(laneId);
    if (tIdx < 0) return;
    const auto& track = tl.tracks[tIdx];
    if (track.stackedLanes) return;

    int laneNum = 0, patId = -1;
    for (int j = 0; j < (int)track.lanes.size(); j++) {
        if (track.lanes[j].id == laneId) { laneNum = j + 1; patId = track.lanes[j].patternId; break; }
    }
    if (patId < 0) return;

    originalLabel.clear();
    for (const auto& p : tl.patterns)
        if (p.id == patId) { originalLabel = p.name; break; }

    editingAbsRow = absRow;
    editingPatId  = patId;

    int ry  = y() + rowYInPanel(absRow);
    int rh  = rowHFor(absRow);
    bool isMultiLane  = (int)track.lanes.size() > 1;
    bool isUnstacked  = !track.stackedLanes;

    int inputY, inputH;
    if (!isUnstacked) {
        // Stacked: pattern name occupies bottom half
        inputY = ry + rh / 2;
        inputH = rh / 2;
    } else {
        // Unstacked: full row for pattern name (instrument name is in header row)
        inputY = ry;
        inputH = rh;
    }

    input.resize(x(), inputY, w(), inputH);
    input.value(originalLabel.c_str());
    input.color(colSelected);
    input.textcolor(colText);
    input.show();
    input.take_focus();
    input.position(input.size(), 0);
    input.onChange([]() {});
    InlineEditDispatch::install(this, [this]() { commitEdit(); });
    redraw();
}

void TrackLabels::commitEdit()
{
    if (editingAbsRow < 0) return;
    int absRow    = editingAbsRow;
    int patId     = editingPatId;
    editingAbsRow = -1;
    editingPatId  = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = input.value();
    input.hide();
    if (timeline) {
        if (patId >= 0) {
            timeline->setPatternName(patId, newLabel);
        } else {
            const auto& ro = timeline->get().rowOrder;
            int trackId = -1;
            if (absRow < (int)ro.size()) {
                const auto& ref = ro[absRow];
                if (ref.kind == RowKind::Lane)
                    trackId = timeline->trackIdForLaneId(ref.id);
                else if (ref.kind == RowKind::Header)
                    trackId = ref.id;
            }
            if (trackId >= 0) {
                int instrId = 0;
                for (const auto& t : timeline->get().tracks)
                    if (t.id == trackId) { instrId = t.instrumentId; break; }
                bool dup = false;
                for (const auto& instr : timeline->get().instruments)
                    if (instr.id != instrId && instr.name == newLabel) { dup = true; break; }
                timeline->renameInstrument(instrId, dup ? originalLabel : newLabel);
            }
        }
    }
    redraw();
}

void TrackLabels::cancelEdit()
{
    if (editingAbsRow < 0) return;
    editingAbsRow = -1;
    editingPatId  = -1;
    InlineEditDispatch::uninstall();
    input.hide();
    redraw();
}

void TrackLabels::draw()
{
    if (!timeline) { draw_children(); return; }

    const auto& tl  = timeline->get();
    const auto& ro  = tl.rowOrder;

    auto patName = [&](int patId) -> std::string {
        for (const auto& p : tl.patterns)
            if (p.id == patId) return p.name;
        return {};
    };

    fl_push_clip(x(), y(), w(), h());   // partial top/bottom rows must not overdraw neighbours
    fl_font(FL_HELVETICA, 11);
    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        if (i == editingAbsRow && editingPatId < 0) continue;  // track-label input draws the whole row itself
        int rh = rowHFor(i);
        int ry = y() + rowYInPanel(i);

        if (i >= 0 && i < (int)ro.size()) {
            const auto& ref = ro[i];
            bool isDragSrc;
            if (dragging && isTrackDrag && dragTrackId >= 0) {
                isDragSrc = (ref.kind == RowKind::Header && ref.id == dragTrackId) ||
                            (ref.kind == RowKind::Lane && timeline->trackIdForLaneId(ref.id) == dragTrackId);
            } else {
                isDragSrc = dragging && i == dragRow;
            }

            if (ref.kind == RowKind::Header) {
                // ── Dedicated instrument name header row ──
                fl_color(isDragSrc ? fl_color_average(colInstrHeader, FL_WHITE, 0.75f) : colInstrHeader);
                fl_rectf(x(), ry, w(), rh);
                // Strong divider at top (between tracks)
                if (ry > y()) {
                    fl_color(colTrackDiv);
                    fl_rectf(x(), ry - 1, w(), 2);
                }
                // Down-arrow collapse icon
                {
                    fl_color(colNormal);
                    int ax = x() + 3, ay = ry + rh / 2 - 3;
                    fl_polygon(ax, ay, ax + 8, ay, ax + 4, ay + 6);
                }
                // Find the track name by track ID
                for (const auto& t : tl.tracks) {
                    if (t.id == ref.id) {
                        fl_font(FL_HELVETICA_BOLD, 9);
                        fl_color(colNormal);
                        fl_draw(tl.instrumentName(t.instrumentId).c_str(), x() + iconAreaW, ry, w() - iconAreaW - 4, rh,
                                FL_ALIGN_LEFT | FL_ALIGN_CLIP | FL_ALIGN_INSIDE);
                        break;
                    }
                }
            } else if (ref.kind == RowKind::Lane) {
                // ref.id is a Lane ID — find the owning Track
                bool isSel = (ref.id == tl.selectedLaneId);
                Fl_Color bg = isSel ? colSelected : colNormal;
                if (isDragSrc) bg = fl_color_average(bg, FL_WHITE, 0.75f);
                fl_color(bg);
                fl_rectf(x(), ry, w(), rh);
                fl_color(colBorder);
                fl_line(x(), ry + rh - 1, x() + w() - 1, ry + rh - 1);
                int tIdx = timeline->trackIndexForLaneId(ref.id);
                if (tIdx >= 0) {
                    const auto& track = tl.tracks[tIdx];
                    bool isFirstLane = (!track.lanes.empty() && track.lanes[0].id == ref.id);
                    bool isMultiLane = (int)track.lanes.size() > 1;
                    bool isUnstacked = !track.stackedLanes;

                    // Track boundary divider for stacked tracks only (unstacked tracks use header row)
                    if (isFirstLane && ry > y() && !isUnstacked) {
                        fl_color(colTrackDiv);
                        fl_rectf(x(), ry - 1, w(), 2);
                    }

                    int laneNum = 1;
                    for (int j = 0; j < (int)track.lanes.size(); j++)
                        if (track.lanes[j].id == ref.id) { laneNum = j + 1; break; }

                    if (isUnstacked) {
                        // ── Unstacked lane: just pattern name (instrument name is in header row) ──
                        fl_font(FL_HELVETICA, 11);
                        fl_color(isDragSrc ? fl_color_average(colText, FL_WHITE, 0.5f) : colText);
                        fl_draw(patName(track.lanes[laneNum-1].patternId).c_str(),
                                x() + 4, ry, w() - 8, rh,
                                FL_ALIGN_LEFT | FL_ALIGN_CLIP);
                    } else if (isFirstLane) {
                        // ── Stacked first lane: right-arrow expand icon + track label ──
                        Fl_Color arrowCol = isDragSrc ? fl_color_average(colText, FL_WHITE, 0.5f) : colText;
                        fl_color(arrowCol);
                        int ax = x() + 4, ay = ry + rh / 2 - 4;
                        fl_polygon(ax, ay, ax, ay + 8, ax + 6, ay + 4);
                        fl_font(FL_HELVETICA_BOLD, 9);
                        fl_draw(tl.instrumentName(track.instrumentId).c_str(), x() + iconAreaW, ry, w() - iconAreaW - 4, rh,
                                FL_ALIGN_LEFT | FL_ALIGN_CLIP | FL_ALIGN_INSIDE);
                    }
                }
            } else {
                Fl_Color bg = isDragSrc ? fl_color_average(colParam, FL_WHITE, 0.75f) : colParam;
                fl_color(bg);
                fl_rectf(x(), ry, w(), rh);
                fl_color(colBorder);
                fl_line(x(), ry + rh - 1, x() + w() - 1, ry + rh - 1);
                fl_color(isDragSrc ? fl_color_average(colText, FL_WHITE, 0.5f) : colText);
                for (const auto& lane : tl.paramLanes)
                    if (lane.id == ref.id) {
                        fl_draw(lane.type.c_str(), x() + 4, ry, w() - 8, rh,
                                FL_ALIGN_LEFT | FL_ALIGN_CLIP);
                        break;
                    }
            }
        } else {
            fl_color(colEmpty);
            fl_rectf(x(), ry, w(), rh);
            fl_color(colBorder);
            fl_line(x(), ry + rh - 1, x() + w() - 1, ry + rh - 1);
        }
    }

    // Drop indicator line (suppressed when track-dragging over itself or when the
    // drop is forbidden by drum/non-drum compatibility)
    if (dragging && dropRow >= 0 && !dropForbidden && !(isTrackDrag && dropTrackId == dragTrackId)) {
        int lineY = y() + rowYInPanel(dropRow);
        lineY = std::clamp(lineY, y(), y() + h());
        fl_color(0x00BFFFFF);  // cyan
        fl_line_style(FL_SOLID, 2);
        fl_line(x(), lineY, x() + w() - 1, lineY);
        fl_line_style(0);
    }

    fl_pop_clip();
    draw_children();
}

int TrackLabels::handle(int event)
{
    if (int r = widgetCursorHandle(this, event, contextMenuCursorImage()); r >= 0) return r;

    if (Fl_Group::handle(event))
        return 1;

    if (event == FL_PUSH) {
        if (!timeline) return 1;
        int row = absRowAtPanelY(Fl::event_y() - y());
        const auto& ro = timeline->get().rowOrder;

        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (row >= 0 && row < (int)ro.size()) {
                const auto& ref = ro[row];
                if (ref.kind == RowKind::Header) {
                    // Right-click instrument header → show track context menu
                    if (contextPopup && patternObs) {
                        contextPopup->open(ref.id, -1, patternObs,
                                           Fl::event_x(), Fl::event_y());
                    }
                } else if (ref.kind == RowKind::Lane) {
                    if (contextPopup && patternObs) {
                        int trackId = timeline->trackIdForLaneId(ref.id);
                        if (trackId >= 0)
                            contextPopup->open(trackId, ref.id, patternObs,
                                               Fl::event_x(), Fl::event_y());
                    }
                } else {
                    if (paramLanePopup && patternObs)
                        paramLanePopup->open(ref.id, patternObs,
                                             Fl::event_x(), Fl::event_y());
                }
            }
            return 1;
        }

        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (editingAbsRow >= 0 && row != editingAbsRow)
                commitEdit();

            if (Fl::event_clicks() > 0) {
                if (row < (int)ro.size()) {
                    const auto& ref = ro[row];
                    if (ref.kind == RowKind::Header) {
                        startInstrumentEdit(row);
                    } else if (ref.kind == RowKind::Lane) {
                        int tIdx = timeline->trackIndexForLaneId(ref.id);
                        bool stackedFirst = tIdx >= 0 && [&]() {
                            const auto& track = timeline->get().tracks[tIdx];
                            return track.stackedLanes && !track.lanes.empty() && track.lanes[0].id == ref.id;
                        }();
                        if (stackedFirst)
                            startInstrumentEdit(row);
                        else
                            startPatternEdit(row);
                    }
                }
                return 1;
            }

            if (row < (int)ro.size() && ro[row].kind == RowKind::Header) {
                // Click on collapse icon → collapse to stacked
                if (Fl::event_x() < x() + iconAreaW) {
                    timeline->setStackedLanes(ro[row].id, true);
                    return 1;
                }
                // Dragging from an instrument header moves the whole track
                dragStartRow = row;
                dragStartY   = Fl::event_y();
                dragging     = false;
                dragRow      = -1;
                dropRow      = -1;
                isTrackDrag   = true;
                dragTrackId   = ro[row].id;
                dropTrackId   = -1;
                dropForbidden = false;
                return 1;
            }

            // Click on expand icon in stacked first-lane row → expand to unstacked
            if (Fl::event_x() < x() + iconAreaW && row < (int)ro.size() && ro[row].kind == RowKind::Lane) {
                int tIdx = timeline->trackIndexForLaneId(ro[row].id);
                if (tIdx >= 0) {
                    const auto& t = timeline->get().tracks[tIdx];
                    if (t.stackedLanes && !t.lanes.empty() && t.lanes[0].id == ro[row].id) {
                        timeline->setStackedLanes(t.id, false);
                        return 1;
                    }
                }
            }

            dragStartRow = row;
            dragStartY   = Fl::event_y();
            dragging     = false;
            dragRow      = -1;
            dropRow      = -1;
            isTrackDrag  = false;
            dragTrackId  = -1;
            dropTrackId  = -1;
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
            if (isTrackDrag) {
                // Build visible track blocks and snap drop to track boundaries
                struct TBound { int trackId; int startY; int endY; };
                std::vector<TBound> bounds;
                std::set<int> seen;
                for (int i = rowOffset; i < rowOffset + numVisibleRows && i < (int)ro.size(); i++) {
                    const auto& r = ro[i];
                    int tid = -1;
                    if (r.kind == RowKind::Header) tid = r.id;
                    else if (r.kind == RowKind::Lane) tid = timeline->trackIdForLaneId(r.id);
                    if (tid >= 0 && !seen.count(tid)) {
                        seen.insert(tid);
                        bounds.push_back({tid, rowYInPanel(i), 0});
                    }
                }
                for (int k = 0; k < (int)bounds.size(); k++)
                    bounds[k].endY = (k + 1 < (int)bounds.size()) ? bounds[k+1].startY : h();

                dropTrackId = -1;
                for (const auto& b : bounds) {
                    if (mouseRelY < (b.startY + b.endY) / 2) {
                        dropTrackId = b.trackId;
                        break;
                    }
                }

                // Find the rowOrder index for the drop indicator line
                dropRow = (int)ro.size();
                if (dropTrackId >= 0) {
                    std::set<int> dropLaneIds;
                    for (const auto& t : timeline->get().tracks)
                        if (t.id == dropTrackId)
                            for (const auto& l : t.lanes) dropLaneIds.insert(l.id);
                    for (int i = 0; i < (int)ro.size(); i++) {
                        if (ro[i].kind == RowKind::Header && ro[i].id == dropTrackId) { dropRow = i; break; }
                        if (ro[i].kind == RowKind::Lane && dropLaneIds.count(ro[i].id)) { dropRow = i; break; }
                    }
                }

                // Forbid dropping between drum and non-drum instruments.
                dropForbidden = !trackDropAllowed();
                window()->cursor(dropForbidden ? forbiddenCursorImage()
                                               : contextMenuCursorImage(), 0, 0);
            } else {
                int cumY = 0;
                int gap = rowOffset;
                for (int i = rowOffset; i < rowOffset + numVisibleRows && i < (int)ro.size(); i++) {
                    int rh = rowHFor(i);
                    int midY = cumY + rh / 2;
                    if (mouseRelY <= midY) break;
                    cumY += rh;
                    gap = i + 1;
                }
                dropRow = std::clamp(gap, 0, (int)ro.size());

                // Forbid transferring a lane to an instrument of the opposite kind
                // (drum vs non-drum). Pure reordering within a track is fine.
                dropForbidden = false;
                if (dragRow >= 0 && dragRow < (int)ro.size() && ro[dragRow].kind == RowKind::Lane) {
                    int srcTrack  = timeline->trackIdForLaneId(ro[dragRow].id);
                    int destTrack = timeline->predictRowDropTrack(dragRow, dropRow);
                    if (srcTrack >= 0 && destTrack >= 0 && destTrack != srcTrack &&
                        trackIsDrum(srcTrack) != trackIsDrum(destTrack))
                        dropForbidden = true;
                }
                window()->cursor(dropForbidden ? forbiddenCursorImage()
                                               : contextMenuCursorImage(), 0, 0);
            }
            redraw();
        }
        return 1;
    }

    if (event == FL_RELEASE) {
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (dragging) {
                // A forbidden drop reverts: leave the row where it was.
                if (timeline && dragRow >= 0 && dropRow >= 0 && !dropForbidden) {
                    if (isTrackDrag)
                        timeline->moveTrack(dragTrackId, dropTrackId);
                    else
                        timeline->moveRow(dragRow, dropRow);
                }
                dragging      = false;
                dragRow       = -1;
                dropRow       = -1;
                isTrackDrag   = false;
                dragTrackId   = -1;
                dropTrackId   = -1;
                dropForbidden = false;
            } else if (dragStartRow >= 0 && timeline) {
                // Pure single click: select lane (skip instrument header rows)
                const auto& ro = timeline->get().rowOrder;
                if (dragStartRow < (int)ro.size() && ro[dragStartRow].kind == RowKind::Lane) {
                    int laneId   = ro[dragStartRow].id;
                    int trackIdx = timeline->trackIndexForLaneId(laneId);
                    if (trackIdx >= 0) timeline->selectLane(trackIdx, laneId);
                }
            }
            dragStartRow  = -1;
            isTrackDrag   = false;
            dragTrackId   = -1;
            dropTrackId   = -1;
            dropForbidden = false;
            return 1;
        }
    }

    if (event == FL_KEYDOWN && editingAbsRow >= 0) {
        if (Fl::event_key() == FL_Escape) { cancelEdit(); return 1; }
    }

    return 0;
}
