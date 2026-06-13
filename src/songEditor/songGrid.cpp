#include "songGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include "cursors.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <ranges>

static constexpr Fl_Color kParamRowBg      = 0xF0F4FF00;
static constexpr Fl_Color kParamLine       = 0x8888CC00;
static constexpr Fl_Color kParamDotFill    = 0x5555EE00;
static constexpr Fl_Color kParamDotRim     = 0x1111EE00;
static constexpr Fl_Color kTrackDiv        = 0x64748B00;
static constexpr Fl_Color kInstrHeaderBg   = 0x64748B00;  // same slate-blue as dividers
static constexpr Fl_Color kBlockFill       = 0x5555EE00;
static constexpr Fl_Color kBlockBar        = 0x1111EE00;
static constexpr float    kStackOpacity    = 0.6f;

SongGrid::SongGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
    : Grid(numRows, numCols, rowHeight, colWidth, snap, popup)
{}

bool SongGrid::isInstrHeaderVR(int vr) const
{
    if (!timeline) return false;
    int absRow = vr + rowOffset;
    const auto& ro = timeline->get().rowOrder;
    return absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Header;
}

int SongGrid::rowY(int r) const
{
    int py = 0;
    for (int i = 0; i < r; i++)
        py += isInstrHeaderVR(i) ? instrNameRowH : rowHeight;
    return py;
}

int SongGrid::rowH(int r) const
{
    return isInstrHeaderVR(r) ? instrNameRowH : rowHeight;
}

int SongGrid::rowAtPixelY(int py) const
{
    int cumY = 0;
    for (int r = 0; r < numRows; r++) {
        int rh = rowH(r);
        if (py < cumY + rh) return r;
        cumY += rh;
    }
    return numRows;
}

int SongGrid::totalPixelH() const
{
    int h = 0;
    for (int r = 0; r < numRows; r++) h += rowH(r);
    return h;
}

void SongGrid::drawNoteBlock(const Note& /*note*/, int x0, int y0, int width, int rh)
{
    fl_rectf(x0, y0 + 1, width, rh - 1, kBlockFill);
    const int barWidth = 5;
    fl_color(kBlockBar);
    fl_line_style(FL_SOLID, barWidth);
    fl_line(x0 + barWidth / 2, y0 + 1, x0 + barWidth / 2, y0 + rh - 1);
    fl_line_style(0);
}

void SongGrid::draw()
{
    Grid::draw();
    if (!timeline) return;

    fl_push_clip(x(), y(), w(), h());

    // Stacked-mode block overdraw: simulate opacity compositing
    // K=1 opacity (single layer): 1-(1-α)^1 = α
    // K=2 opacity (two overlapping layers): 1-(1-α)^2
    if (!stackedNoteIds.empty()) {
        int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
        const Fl_Color k1Fill = fl_color_average(kBlockFill, bgColor, kStackOpacity);
        const Fl_Color k2Fill = fl_color_average(kBlockFill, bgColor, 1.0f - (1.0f - kStackOpacity) * (1.0f - kStackOpacity));
        const Fl_Color k1Bar  = fl_color_average(kBlockBar,  bgColor, kStackOpacity);
        const int barWidth = 5;

        // Pass 1: overwrite stacked fills with K=1 opacity color
        for (const auto& note : notes) {
            if (note.row < 0 || note.row >= numRows) continue;
            if (!stackedNoteIds.count(note.id)) continue;
            int x0    = x() + (int)((note.beat - colOffset) * colWidth);
            int y0    = y() + rowY((int)note.row);
            int rh    = rowH((int)note.row);
            int width = (int)(note.length * colWidth);
            if (x0 + width < x() || x0 > x() + gridRight) continue;
            fl_rectf(x0, y0 + 1, width, rh - 1, k1Fill);
        }

        // Pass 2: overwrite pairwise overlap rectangles with K=2 opacity color
        const int n = (int)notes.size();
        for (int i = 0; i < n; i++) {
            if (!stackedNoteIds.count(notes[i].id)) continue;
            for (int j = i + 1; j < n; j++) {
                if (!stackedNoteIds.count(notes[j].id)) continue;
                if (notes[i].row != notes[j].row) continue;
                float oStart = std::max(notes[i].beat, notes[j].beat);
                float oEnd   = std::min(notes[i].beat + notes[i].length, notes[j].beat + notes[j].length);
                if (oEnd <= oStart) continue;
                int ox0 = x() + (int)((oStart - colOffset) * colWidth);
                int ow  = (int)((oEnd - oStart) * colWidth);
                int y0  = y() + rowY((int)notes[i].row);
                int rh  = rowH((int)notes[i].row);
                if (ox0 + ow < x() || ox0 > x() + gridRight || ow <= 0) continue;
                fl_rectf(ox0, y0 + 1, ow, rh - 1, k2Fill);
            }
        }

        // Pass 3: redraw stacked bars on top
        for (const auto& note : notes) {
            if (note.row < 0 || note.row >= numRows) continue;
            if (!stackedNoteIds.count(note.id)) continue;
            int x0 = x() + (int)((note.beat - colOffset) * colWidth);
            int y0 = y() + rowY((int)note.row);
            int rh = rowH((int)note.row);
            if (x0 > x() + gridRight || x0 + 20 < x()) continue;
            fl_color(k1Bar);
            fl_line_style(FL_SOLID, barWidth);
            fl_line(x0 + barWidth / 2, y0 + 1, x0 + barWidth / 2, y0 + rh - 1);
            fl_line_style(0);
        }
    }

    // Track divider lines — strong horizontal line above each track's first row
    {
        const auto& tl = timeline->get();
        const auto& ro = tl.rowOrder;
        int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
        for (int vr = 1; vr < numRows; vr++) {
            int absRow = vr + rowOffset;
            if (absRow < 0 || absRow >= (int)ro.size()) continue;
            bool drawDiv = false;
            if (ro[absRow].kind == RowKind::Header) {
                drawDiv = true;
            } else if (ro[absRow].kind == RowKind::Lane) {
                int tIdx = timeline->trackIndexForLaneId(ro[absRow].id);
                if (tIdx < 0) continue;
                const auto& t = tl.tracks[tIdx];
                if (t.lanes.empty() || t.lanes[0].id != ro[absRow].id) continue;
                // Only draw divider for stacked tracks (header handles unstacked)
                drawDiv = t.stackedLanes;
            }
            if (!drawDiv) continue;
            int lineY = y() + rowY(vr) - 1;
            fl_color(kTrackDiv);
            fl_rectf(x(), lineY, gridRight, 2);
        }
    }
    const int tickH = 4;
    for (const auto& note : notes) {
        if (note.row < 0 || note.row >= numRows) continue;
        int y0  = y() + rowY((int)note.row);
        int rh  = rowH((int)note.row);
        int top = 4, bottom = 4;
        timeline->timeSigAt((int)note.beat, top, bottom);

        float startOffset = 0.0f;
        // While left-resizing, show dragStartOffset so the tick stays fixed visually
        if (auto* s = std::get_if<StateDragResize>(&state))
            if (s->side == Side::Left && note.id == notes[s->noteIdx].id)
                startOffset = dragStartOffset;
        if (startOffset == 0.0f)
            if (const PatternInstance* inst = timeline->instanceById(note.id))
                startOffset = inst->startOffset;

        float beatZeroPos = note.beat - startOffset / top;
        float instanceEnd = note.beat + note.length;

        float intervalBars = 0.0f;
        if (const Pattern* pat = timeline->patternForInstance(note.id))
            if (pat->lengthBeats > 0.0f)
                intervalBars = pat->lengthBeats / top;

        float firstTick = beatZeroPos;
        if (intervalBars > 0.0f) {
            float k = std::ceil((note.beat - beatZeroPos) / intervalBars);
            firstTick = beatZeroPos + k * intervalBars;
        }

        fl_color(FL_WHITE);
        for (float tickBar = firstTick; tickBar < instanceEnd; ) {
            int tickX = x() + (int)((tickBar - colOffset) * colWidth);
            fl_rectf(tickX, y0 + 1,           2, tickH);
            fl_rectf(tickX, y0 + rh - 1 - tickH, 2, tickH);
            if (intervalBars <= 0.0f) break;
            tickBar += intervalBars;
        }
    }

    // Pattern instance names — small white text at top-left of each block
    {
        int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
        fl_font(FL_HELVETICA, 10);
        for (const auto& note : notes) {
            if (note.row < 0 || note.row >= numRows) continue;
            const Pattern* pat = timeline->patternForInstance(note.id);
            if (!pat || pat->name.empty()) continue;

            int x0    = x() + (int)((note.beat - colOffset) * colWidth);
            int y0    = y() + rowY((int)note.row);
            int rh    = rowH((int)note.row);
            int width = (int)(note.length * colWidth);

            int clipX = std::max(x(), x0);
            int clipW = std::min(x0 + width, x() + gridRight) - clipX;
            if (clipW <= 4) continue;

            fl_push_clip(clipX, y0 + 1, clipW, rh - 2);
            fl_color(FL_WHITE);
            fl_draw(pat->name.c_str(), x0 + 7, y0 + 2, width - 9, rh - 4,
                    FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_CLIP | FL_ALIGN_INSIDE);
            fl_pop_clip();
        }
    }

    // Param lane dots and rubber bands
    if (!localParamLanes.empty()) {
        int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
        for (int li = 0; li < (int)localParamLanes.size(); li++) {
            int vr = visualRowForLaneId(localParamLanes[li].id);
            if (vr < 0 || vr >= numRows) continue;
            drawParamRow(li, y() + rowY(vr), gridRight);
        }
    }

    fl_pop_clip();
}

SongGrid::~SongGrid()
{
    swapObserver(timeline, nullptr, this);
}

void SongGrid::setTimeline(ObservableSong* tl)
{
    swapObserver(timeline, tl, this);
    rebuildNotes();
    rebuildParamLanes();
    redraw();
}

void SongGrid::rebuildParamLanes()
{
    localParamLanes.clear();
    if (!timeline) return;
    for (const auto& lane : timeline->get().paramLanes) {
        ParamLaneLocal local;
        local.id   = lane.id;
        local.type = lane.type;
        for (const auto& pt : lane.points)
            local.points.push_back({pt.id, pt.beat, pt.value, pt.anchor});
        localParamLanes.push_back(std::move(local));
    }
}

int SongGrid::visualRowForLaneId(int laneId) const
{
    if (!timeline) return -1;
    const auto& ro = timeline->get().rowOrder;
    for (int i = 0; i < (int)ro.size(); i++)
        if (ro[i].kind == RowKind::Param && ro[i].id == laneId)
            return i - rowOffset;
    return -1;
}

int SongGrid::laneIdxForAbsRow(int absRow) const
{
    if (!timeline) return -1;
    const auto& ro = timeline->get().rowOrder;
    if (absRow < 0 || absRow >= (int)ro.size() || ro[absRow].kind != RowKind::Param) return -1;
    int id = ro[absRow].id;
    for (int i = 0; i < (int)localParamLanes.size(); i++)
        if (localParamLanes[i].id == id) return i;
    return -1;
}

bool SongGrid::isRowBlocked(int visualRow) const
{
    return isInstrHeaderVR(visualRow);
}

Fl_Color SongGrid::rowBgColor(int visualRow) const
{
    if (!timeline) return bgColor;
    int absRow = visualRow + rowOffset;
    const auto& ro = timeline->get().rowOrder;
    if (absRow >= 0 && absRow < (int)ro.size()) {
        if (ro[absRow].kind == RowKind::Header) return kInstrHeaderBg;
        if (ro[absRow].kind == RowKind::Param)  return kParamRowBg;
    }
    return bgColor;
}


void SongGrid::drawParamRow(int laneIdx, int rowY, int gridRight)
{
    const auto& lane = localParamLanes[laneIdx];
    const int dotR = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    if (totalRange <= 0) return;
    const int maxVal = laneMaxValue(lane.type);

    auto dotYFor = [&](int value) {
        return rowY + dotR + (int)((maxVal - value) * totalRange / (float)maxVal);
    };

    // Virtual dot: hollow dotted circle at left edge, value from last off-screen dot
    {
        int predIdx = findPrecedingDotIdx(laneIdx);
        bool draw = predIdx >= 0;
        if (draw) {
            for (const auto& pt : lane.points) {
                int dotX = x() + (int)((pt.beat - colOffset) * colWidth);
                if (std::abs(dotX - x()) < 2 * dotR) { draw = false; break; }
            }
        }
        if (draw) {
            int vdotY = dotYFor(lane.points[predIdx].value);
            fl_color(0xFF999900);
            fl_pie(x() - dotR, vdotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
        }
    }

    fl_color(kParamLine);

    // Rubber band lines between consecutive dots
    for (int i = 0; i + 1 < (int)lane.points.size(); i++) {
        const auto& a = lane.points[i];
        const auto& b = lane.points[i + 1];
        fl_line(x() + (int)((a.beat - colOffset) * colWidth), dotYFor(a.value),
                x() + (int)((b.beat - colOffset) * colWidth), dotYFor(b.value));
    }

    // Extend horizontally from the last dot to the right edge
    if (!lane.points.empty()) {
        const auto& last = lane.points.back();
        int lastX = x() + (int)((last.beat - colOffset) * colWidth);
        int lastY = dotYFor(last.value);
        if (lastX < x() + gridRight)
            fl_line(lastX, lastY, x() + gridRight, lastY);
    }

    // Dots
    for (const auto& pt : lane.points) {
        int dotX = x() + (int)((pt.beat - colOffset) * colWidth);
        if (dotX + dotR < x() || dotX - dotR > x() + w()) continue;
        int dotY = dotYFor(pt.value);
        fl_color(kParamDotFill);
        fl_pie(dotX - dotR, dotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
        fl_color(kParamDotRim);
        fl_arc(dotX - dotR, dotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
    }
}

int SongGrid::findParamPointAtCursor(int laneIdx) const
{
    if (laneIdx < 0 || laneIdx >= (int)localParamLanes.size() || !timeline) return -1;
    int vr = visualRowForLaneId(localParamLanes[laneIdx].id);
    if (vr < 0 || vr >= numRows) return -1;

    const int dotR      = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    const int hitR      = dotR + 4;

    int pRowY = y() + SongGrid::rowY(vr);
    int ex    = Fl::event_x();
    int ey    = Fl::event_y();

    int   bestIdx  = -1;
    float bestDist = (float)(hitR + 1);

    for (int i = 0; i < (int)localParamLanes[laneIdx].points.size(); i++) {
        const auto& pt = localParamLanes[laneIdx].points[i];
        int dotX = x() + (int)((pt.beat - colOffset) * colWidth);
        const int mv = laneMaxValue(localParamLanes[laneIdx].type);
        int dotY = pRowY + dotR + (totalRange > 0 ? (int)((mv - pt.value) * totalRange / (float)mv) : 0);
        float dx = (float)(ex - dotX);
        float dy = (float)(ey - dotY);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < (float)hitR && dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

bool SongGrid::canPlaceDot(int laneIdx, float beat, int excludeId) const
{
    if (beat == 0.0f) return false;
    if (laneIdx < 0 || laneIdx >= (int)localParamLanes.size()) return false;
    int count = 0;
    for (const auto& pt : localParamLanes[laneIdx].points) {
        if (pt.id == excludeId) continue;
        if (pt.beat == beat) count++;
    }
    return count < 2;
}

int SongGrid::findPrecedingDotIdx(int laneIdx) const
{
    if (laneIdx < 0 || laneIdx >= (int)localParamLanes.size()) return -1;
    const auto& pts = localParamLanes[laneIdx].points;
    int best = -1;
    for (int i = 0; i < (int)pts.size(); i++) {
        if (pts[i].beat < (float)colOffset)
            best = i;
    }
    return best;
}

int SongGrid::handleParamEvent(int event)
{
    const int dotR       = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    const int hitR       = dotR + 4;

    int ey      = Fl::event_y() - y();
    int vr      = rowAtPixelY(ey);
    int laneIdx = laneIdxForAbsRow(vr + rowOffset);

    // Helper: check if any real dot overlaps the virtual dot position (left edge)
    auto isVirtualOverlapped = [&](int li) {
        for (const auto& pt : localParamLanes[li].points) {
            int dotX = x() + (int)((pt.beat - colOffset) * colWidth);
            if (std::abs(dotX - x()) < 2 * dotR) return true;
        }
        return false;
    };

    // Helper: absolute y of virtual dot for a given lane
    auto virtualDotY = [&](int li, int predIdx) {
        int laneVR  = visualRowForLaneId(localParamLanes[li].id);
        int pRowY   = y() + (laneVR >= 0 ? SongGrid::rowY(laneVR) : 0);
        int value   = localParamLanes[li].points[predIdx].value;
        int mv      = laneMaxValue(localParamLanes[li].type);
        return pRowY + dotR + (totalRange > 0 ? (int)((mv - value) * totalRange / (float)mv) : 0);
    };

    switch (event) {
    case FL_PUSH: {
        int ex        = Fl::event_x() - x();
        int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
        if (ex >= gridRight) { paramState = ParamIdle{}; return 1; }
        if (laneIdx < 0 || laneIdx >= (int)localParamLanes.size()) return 1;

        int ptIdx = findParamPointAtCursor(laneIdx);

        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (ptIdx >= 0 && paramDotPopup) {
                auto& pt   = localParamLanes[laneIdx].points[ptIdx];
                int   ptId = pt.id;
                float beat = pt.beat;
                int   val  = pt.value;
                bool  anc  = pt.anchor;
                int maxVal = laneMaxValue(localParamLanes[laneIdx].type);
                paramState = ParamIdle{};
                paramDotPopup->open(Fl::event_x(), Fl::event_y(), val, anc, maxVal,
                    [this, ptId, beat](int newVal) {
                        if (timeline) timeline->moveParamPoint(ptId, beat, newVal);
                    },
                    [this, ptId, anc]() {
                        if (timeline && !anc) timeline->removeParamPoint(ptId);
                    });
            } else {
                paramState = ParamIdle{};
            }
            return 1;
        }
        if (ptIdx >= 0) {
            auto& pt = localParamLanes[laneIdx].points[ptIdx];
            paramState = ParamDragState{laneIdx, ptIdx, pt.beat, pt.value};
            if (window()) window()->cursor(FL_CURSOR_HAND);
        } else {
            // Check virtual dot before creating a new one
            bool hitVirtual = false;
            int predIdx = findPrecedingDotIdx(laneIdx);
            if (predIdx >= 0 && !isVirtualOverlapped(laneIdx)) {
                int vdotY = virtualDotY(laneIdx, predIdx);
                float dx = (float)(Fl::event_x() - x());
                float dy = (float)(Fl::event_y() - vdotY);
                if (std::sqrt(dx * dx + dy * dy) <= (float)hitR) {
                    int origVal = localParamLanes[laneIdx].points[predIdx].value;
                    paramState = ParamVirtualDrag{laneIdx, predIdx, origVal};
                    if (window()) window()->cursor(FL_CURSOR_HAND);
                    hitVirtual = true;
                }
            }
            if (!hitVirtual) {
                int maxVal  = laneIdx >= 0 && laneIdx < (int)localParamLanes.size()
                              ? laneMaxValue(localParamLanes[laneIdx].type) : 127;
                float beat  = (float)ex / colWidth + colOffset;
                if (snap > 0.0f) beat = std::round(beat / snap) * snap;
                beat = std::max(0.0f, beat);
                int eyInRow = ey - SongGrid::rowY(vr);
                int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
                int value   = totalRange > 0 ? maxVal - (int)(mapped * (float)maxVal / totalRange) : maxVal / 2;
                paramState  = ParamPendingCreate{laneIdx, beat, std::clamp(value, 0, maxVal)};
            }
        }
        return 1;
    }

    case FL_DRAG: {
        if (auto* d = std::get_if<ParamVirtualDrag>(&paramState)) {
            int maxVal  = laneMaxValue(localParamLanes[d->laneIdx].type);
            int laneVR  = visualRowForLaneId(localParamLanes[d->laneIdx].id);
            int eyInRow = ey - (laneVR >= 0 ? SongGrid::rowY(laneVR) : 0);
            int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
            int newVal  = totalRange > 0 ? maxVal - (int)(mapped * (float)maxVal / totalRange) : maxVal / 2;
            newVal = std::clamp(newVal, 0, maxVal);
            auto& pt = localParamLanes[d->laneIdx].points[d->predPtIdx];
            if (newVal != pt.value) d->moved = true;
            pt.value = newVal;
            redraw();
            return 1;
        }
        if (auto* d = std::get_if<ParamDragState>(&paramState)) {
            int ex        = Fl::event_x() - x();
            bool isAnchor = localParamLanes[d->laneIdx].points[d->ptIdx].anchor;

            float newBeat = (float)ex / colWidth + colOffset;
            if (snap > 0.0f) newBeat = std::round(newBeat / snap) * snap;
            newBeat = std::max(0.0f, std::min((float)numCols, newBeat));
            if (isAnchor) {
                newBeat = 0.0f;
            } else {
                if (snap > 0.0f)
                    newBeat = std::max(snap, newBeat);
                // Clamp between neighbors so the dot can't pass them
                const auto& pts = localParamLanes[d->laneIdx].points;
                float lo = pts[d->ptIdx - 1].beat;
                float hi = (d->ptIdx + 1 < (int)pts.size()) ? pts[d->ptIdx + 1].beat
                                                             : (float)numCols;
                newBeat = std::clamp(newBeat, lo, hi);
            }

            int maxVal   = laneMaxValue(localParamLanes[d->laneIdx].type);
            int laneVR   = visualRowForLaneId(localParamLanes[d->laneIdx].id);
            int eyInRow  = ey - (laneVR >= 0 ? SongGrid::rowY(laneVR) : 0);
            int mapped   = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
            int newValue = totalRange > 0 ? maxVal - (int)(mapped * (float)maxVal / totalRange) : maxVal / 2;
            newValue     = std::clamp(newValue, 0, maxVal);

            auto& pt = localParamLanes[d->laneIdx].points[d->ptIdx];
            if (newBeat != pt.beat || newValue != pt.value) d->moved = true;
            if (newBeat != pt.beat && canPlaceDot(d->laneIdx, newBeat, pt.id))
                pt.beat = newBeat;
            pt.value = newValue;
            redraw();
        }
        return 1;
    }

    case FL_RELEASE: {
        if (auto* d = std::get_if<ParamVirtualDrag>(&paramState)) {
            auto& pt   = localParamLanes[d->laneIdx].points[d->predPtIdx];
            int  ptId  = pt.id;
            float beat = pt.beat;
            int  value = pt.value;
            bool moved = d->moved;
            paramState = ParamIdle{};
            if (moved && timeline)
                timeline->moveParamPoint(ptId, beat, value);
            else
                { rebuildParamLanes(); redraw(); }
        } else if (auto* d = std::get_if<ParamDragState>(&paramState)) {
            auto& pt      = localParamLanes[d->laneIdx].points[d->ptIdx];
            int   ptId    = pt.id;
            float beat    = pt.beat;
            int   value   = pt.value;
            bool  moved   = d->moved;
            bool  anchor  = pt.anchor;
            float origBeat = d->origBeat;
            paramState = ParamIdle{};
            if (moved && timeline) {
                float validBeat = canPlaceDot(d->laneIdx, beat, ptId) ? beat : origBeat;
                timeline->moveParamPoint(ptId, validBeat, value);
            } else {
                rebuildParamLanes(); redraw();
            }
        } else if (auto* d = std::get_if<ParamPendingCreate>(&paramState)) {
            int li = d->laneIdx; float beat = d->beat; int value = d->value;
            paramState = ParamIdle{};
            if (timeline && li >= 0 && li < (int)localParamLanes.size() && canPlaceDot(li, beat))
                timeline->addParamPoint(localParamLanes[li].id, beat, value);
            else
                redraw();
        } else {
            paramState = ParamIdle{};
        }
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        return 1;
    }

    case FL_ENTER:
        return 1;

    case FL_MOVE: {
        bool useHand = false;
        if (laneIdx >= 0 && laneIdx < (int)localParamLanes.size()) {
            useHand = findParamPointAtCursor(laneIdx) >= 0;
            if (!useHand) {
                int predIdx = findPrecedingDotIdx(laneIdx);
                if (predIdx >= 0 && !isVirtualOverlapped(laneIdx)) {
                    int vdotY = virtualDotY(laneIdx, predIdx);
                    float dx = (float)(Fl::event_x() - x());
                    float dy = (float)(Fl::event_y() - vdotY);
                    useHand = std::sqrt(dx * dx + dy * dy) <= (float)hitR;
                }
            }
        }
        if (window()) {
            if (useHand) window()->cursor(contextMenuCursorImage(), 0, 0);
            else         window()->cursor(FL_CURSOR_DEFAULT);
        }
        return 0;
    }

    case FL_LEAVE:
        paramState = ParamIdle{};
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        return 0;

    default:
        return 0;
    }
}

void SongGrid::rebuildNotes()
{
    if (!timeline) return;
    stackedNoteIds.clear();
    if (trackFilter >= 0) {
        const auto& tracks = timeline->get().tracks;
        notes.clear();
        if (trackFilter < (int)tracks.size()) {
            float scale = 1.0f;
            if (beatResolution) {
                int top, bottom;
                timeline->timeSigAt(0, top, bottom);
                scale = (float)top;
            }
            if (!tracks[trackFilter].lanes.empty())
                for (const auto& p : tracks[trackFilter].lanes[0].patterns)
                    notes.push_back({p.id, 0, p.startBar * scale, p.length * scale});
        }
        clampSelection();
        return;
    }
    for (const auto& t : timeline->get().tracks) {
        if (!t.stackedLanes) continue;
        for (const auto& l : t.lanes)
            for (const auto& p : l.patterns)
                stackedNoteIds.insert(p.id);
    }
    std::vector<Note> all = timeline->buildNotes();
    notes.clear();
    for (auto n : all) {
        int visual = (int)n.row - rowOffset;
        if (visual >= 0 && visual < numRows) {
            n.row = (float)visual;
            notes.push_back(n);
        }
    }
    clampSelection();
}

void SongGrid::setRowOffset(int offset)
{
    rowOffset = offset;
    if (timeline) { rebuildNotes(); redraw(); }
}

void SongGrid::setTrackView(int tf, bool br)
{
    trackFilter    = tf;
    beatResolution = br;
    if (timeline) { rebuildNotes(); redraw(); }
}

void SongGrid::onTimelineChanged()
{
    if (!isActiveDrag())
        rebuildNotes();
    if (!std::holds_alternative<ParamDragState>(paramState) &&
        !std::holds_alternative<ParamVirtualDrag>(paramState))
        rebuildParamLanes();
    redraw();
}

std::function<void()> SongGrid::makeDeleteCallback(int noteIdx)
{
    if (!timeline) return nullptr;
    int id = notes[noteIdx].id;
    return [this, id]() { timeline->removePattern(id); };
}

void SongGrid::openContextMenu(int idx)
{
    if (!songPopup) { Grid::openContextMenu(idx); return; }
    int absRow   = (int)notes[idx].row + rowOffset;
    int trackIdx = -1;
    int laneId   = -1;
    if (timeline) {
        const auto& ro = timeline->get().rowOrder;
        if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Lane) {
            laneId   = ro[absRow].id;
            trackIdx = timeline->trackIndexForLaneId(laneId);
        }
    }
    songPopup->open(&notes, idx, this,
        makeDeleteCallback(idx),
        (onOpenPattern && trackIdx >= 0)
            ? std::function<void()>([this, trackIdx, laneId]() { onOpenPattern(trackIdx, laneId); })
            : nullptr);
}

int SongGrid::overlappingCell(int noteIdx) const
{
    if (!timeline) return Grid::overlappingCell(noteIdx);
    const Note& a      = notes[noteIdx];
    float       aStart = a.beat, aEnd = a.beat + a.length;
    int         aLane  = timeline->laneIdForInstance(a.id);

    // In unstacked mode each visual row is exactly one lane, so any overlap is
    // forbidden. In stacked mode different lanes share a row, so only same-lane
    // overlaps are forbidden.
    int absRow = (int)a.row + rowOffset;
    bool destUnstacked = false;
    const auto& ro = timeline->get().rowOrder;
    if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Lane) {
        int destTrackIdx = timeline->trackIndexForLaneId(ro[absRow].id);
        if (destTrackIdx >= 0) {
            const auto& tracks = timeline->get().tracks;
            destUnstacked = !tracks[destTrackIdx].stackedLanes;
        }
    }

    for (const auto [i, b] : std::views::enumerate(notes)) {
        if (i == noteIdx || b.row != a.row) continue;
        float bStart = b.beat, bEnd = b.beat + b.length;
        float firstEnd    = aStart <= bStart ? aEnd   : bEnd;
        float secondStart = aStart <= bStart ? bStart : aStart;
        if (firstEnd > secondStart) {
            if (destUnstacked) return i;
            int bLane = timeline->laneIdForInstance(b.id);
            if (aLane < 0 || bLane < 0 || aLane == bLane)
                return i;
        }
    }
    return -1;
}

void SongGrid::onBeginDrag(int noteIdx)
{
    if (timeline) { int dummy; timeline->timeSigAt((int)notes[noteIdx].beat, dragBeatsPerBar, dummy); }
    float startOffset = 0.0f;
    if (const PatternInstance* inst = timeline->instanceById(notes[noteIdx].id))
        startOffset = inst->startOffset;
    tickBarPos = notes[noteIdx].beat - startOffset / dragBeatsPerBar;
}

void SongGrid::moving(StateDragMove& s)
{
    if (timeline) {
        float ex      = Fl::event_x() - x();
        float rawBeat = (ex - s.grabX) / (float)colWidth + colOffset;
        int   bpb, dummy;
        timeline->timeSigAt((int)std::max(0.0f, rawBeat), bpb, dummy);
        snap = 1.0f / bpb;
    }
    Grid::moving(s);
}

void SongGrid::resizing(StateDragResize& s)
{
    if (timeline) {
        float ex      = Fl::event_x() - x();
        float rawBeat = ex / (float)colWidth + colOffset;
        int   bpb, dummy;
        timeline->timeSigAt((int)std::max(0.0f, rawBeat), bpb, dummy);
        snap = 1.0f / bpb;
    }
    Grid::resizing(s);
    if (s.side == Side::Left) {
        float newOffset = (notes[s.noteIdx].beat - tickBarPos) * dragBeatsPerBar;
        dragStartOffset = newOffset;
    }
}

void SongGrid::onCommitMove(const StateDragMove& s)
{
    if (!timeline) return;
    int id     = notes[s.noteIdx].id;
    int absRow = (int)notes[s.noteIdx].row + rowOffset;
    const auto& ro = timeline->get().rowOrder;
    int laneId = -1;
    if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Lane)
        laneId = ro[absRow].id;

    // In stacked mode every lane maps to the same row, so ro[absRow].id is
    // always the first lane. Preserve the instance's original lane when the
    // move stays within the same track in stacked mode; in unstacked mode
    // ro[absRow].id is already the correct destination lane.
    int origLane  = timeline->laneIdForInstance(id);
    int origTrack = origLane >= 0 ? timeline->trackIndexForLaneId(origLane) : -1;
    int destTrack = laneId   >= 0 ? timeline->trackIndexForLaneId(laneId)   : -1;
    if (origTrack >= 0 && origTrack == destTrack) {
        const auto& tracks = timeline->get().tracks;
        if (origTrack < (int)tracks.size() && tracks[origTrack].stackedLanes)
            laneId = origLane;
    }

    timeline->movePattern(id, laneId, notes[s.noteIdx].beat);
}

void SongGrid::onCommitResize(const StateDragResize& s)
{
    if (!timeline) return;
    int id = notes[s.noteIdx].id;
    if (s.side == Side::Left)
        timeline->resizePatternLeft(id, notes[s.noteIdx].beat, notes[s.noteIdx].length, dragStartOffset);
    else
        timeline->resizePattern(id, notes[s.noteIdx].length);
}

int SongGrid::handle(int event)
{
    if (songPopup && songPopup->visible()) return 0;

    // Active param interaction takes priority over everything
    if (!std::holds_alternative<ParamIdle>(paramState))
        return handleParamEvent(event);

    // Route to param handler when cursor is over a param lane row
    if (!localParamLanes.empty() && timeline) {
        int ey  = Fl::event_y() - y();
        int vr  = rowAtPixelY(ey);
        if (laneIdxForAbsRow(vr + rowOffset) >= 0)
            return handleParamEvent(event);
    }

    return Grid::handle(event);
}

void SongGrid::onNoteDoubleClick(int noteIdx)
{
    if (!onPatternDoubleClick || !timeline) return;
    int absRow = (int)notes[noteIdx].row + rowOffset;
    const auto& ro = timeline->get().rowOrder;
    if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Lane) {
        int laneId   = ro[absRow].id;
        int trackIdx = timeline->trackIndexForLaneId(laneId);
        if (trackIdx >= 0) onPatternDoubleClick(trackIdx, laneId);
    }
}

void SongGrid::toggleNote()
{
    if (trackFilter >= 0) return;
    int   ex        = Fl::event_x() - x();
    int   ey        = Fl::event_y() - y();
    int   visualRow = rowAtPixelY(ey);
    int   absRow    = visualRow + rowOffset;
    float col       = (float)(ex / colWidth) + colOffset;

    if (!timeline) { Grid::toggleNote(); return; }

    // Block interaction on instrument header rows
    {
        const auto& ro = timeline->get().rowOrder;
        if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Header)
            return;
    }

    for (auto& n : notes) {
        if ((int)n.row == visualRow && n.beat == col) {
            timeline->removePattern(n.id);
            return;
        }
    }
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return (int)n.row == visualRow && col < n.beat + n.length && col + 1.0f > n.beat; });
    const auto& ro = timeline->get().rowOrder;
    if (clear && absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Lane) {
        int laneId = ro[absRow].id;
        int patId  = 0;
        for (const auto& t : timeline->get().tracks)
            for (const auto& l : t.lanes)
                if (l.id == laneId) { patId = l.patternId; break; }
        if (patId > 0)
            timeline->placePattern(laneId, patId, col, 1.0f);
    }
}
