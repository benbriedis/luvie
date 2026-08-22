// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "songGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include "cursors.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_RGB_Image.H>
#include <algorithm>
#include <cmath>

static constexpr Fl_Color kParamRowBg      = 0xF0F4FF00;
static constexpr Fl_Color kParamLine       = 0x8888CC00;
static constexpr Fl_Color kParamDotFill    = 0x5555EE00;
static constexpr Fl_Color kParamDotRim     = 0x1111EE00;
static constexpr Fl_Color kTrackDiv        = 0x64748B00;
static constexpr Fl_Color kTrackDivOnHeader= 0x37415100;  // darker: divider between two grey header rows (empty instrument) must stay visible
static constexpr Fl_Color kInstrHeaderBg   = 0x64748B00;  // same slate-blue as dividers
static constexpr Fl_Color kBlockFill       = 0x5555EE00;
static constexpr Fl_Color kBlockBar        = 0x1111EE00;
static constexpr float    kStackOpacity    = 0.6f;
// The copy-and-stamp ghost: a wash faint enough to read the grid and any block
// through, and a red outline for a landing spot that would collide.
static constexpr unsigned char kStampFillAlpha = 72;
static constexpr Fl_Color kStampBlocked    = 0xDC262600;

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
    int py = -pixelOffset;
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
    int cumY = -pixelOffset;
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

// Height of an absolute rowOrder entry, independent of the current rowOffset.
int SongGrid::absRowHeight(int absRow) const
{
    if (!timeline) return rowHeight;
    const auto& ro = timeline->get().rowOrder;
    if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Header)
        return instrNameRowH;
    return rowHeight;
}

int SongGrid::fullContentHeight() const
{
    if (!timeline) return 0;
    int total = (int)timeline->get().rowOrder.size();
    int h = 0;
    for (int r = 0; r < total; r++) h += absRowHeight(r);
    return h;
}

void SongGrid::scrollPxToRow(int scrollPx, int& rowOff, int& pxOff) const
{
    rowOff = 0;
    pxOff  = 0;
    if (!timeline) return;
    int total = (int)timeline->get().rowOrder.size();
    int cum = 0;
    for (int r = 0; r < total; r++) {
        int rh = absRowHeight(r);
        if (cum + rh > scrollPx) { rowOff = r; pxOff = scrollPx - cum; return; }
        cum += rh;
    }
    rowOff = std::max(0, total - 1);
}

int SongGrid::rowsToRender(int rowOff, int pxOff, int availH) const
{
    if (!timeline) return 1;
    int total = (int)timeline->get().rowOrder.size();
    int used = -pxOff, count = 0;
    for (int r = rowOff; r < total; r++) {
        used += absRowHeight(r);
        count++;
        if (used >= availH) break;
    }
    return std::max(1, count);
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
            // The grey divider is invisible against a grey header row directly
            // above it (an empty instrument, which is a lone header). Use a darker
            // divider there so two adjacent grey rows don't merge.
            bool prevIsHeader = absRow - 1 >= 0 && absRow - 1 < (int)ro.size() &&
                                ro[absRow - 1].kind == RowKind::Header;
            int lineY = y() + rowY(vr) - 1;
            fl_color(prevIsHeader ? kTrackDivOnHeader : kTrackDiv);
            fl_rectf(x(), lineY, gridRight, 2);
        }
    }
    const int tickH = 4;
    for (const auto& note : notes) {
        if (note.row < 0 || note.row >= numRows) continue;
        int y0  = y() + rowY((int)note.row);
        int rh  = rowH((int)note.row);
        const Pattern* pat = timeline->patternForInstance(note.id);
        // The pattern's beats, not the song's: its signature and beat definition
        // decide how much of a bar one of its beats spans.
        float beatsPerBar = timeline->patternBeatsPerBar(
            (int)note.beat, pat ? pat->id : 0);

        float startOffset = 0.0f;
        // While left-resizing, show dragStartOffset so the tick stays fixed visually
        if (auto* s = std::get_if<StateDragResize>(&state))
            if (s->side == Side::Left && note.id == notes[s->noteIdx].id)
                startOffset = dragStartOffset;
        if (startOffset == 0.0f)
            if (const PatternInstance* inst = timeline->instanceById(note.id))
                startOffset = inst->startOffset;

        float beatZeroPos = note.beat - startOffset / beatsPerBar;
        float instanceEnd = note.beat + note.length;

        float intervalBars = 0.0f;
        if (pat && pat->lengthBeats > 0.0f)
            intervalBars = pat->lengthBeats / beatsPerBar;

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
            if (!selection.empty())
                drawParamSelection(li, y() + rowY(vr));
        }
    }

    // Grid::draw() already outlined the selected blocks, but the stacked-lane
    // overdraw above paints over them, and the band belongs on top of the dots.
    // Both are cheap, so simply redo them last rather than reordering the passes.
    if (!selection.empty()) {
        for (const auto& note : notes) {
            if (!selection.contains(note.id)) continue;
            if (note.row < 0 || note.row >= numRows) continue;
            int xLeft  = (int)std::lround((note.beat - colOffset) * (double)colWidth);
            int xRight = (int)std::lround((note.beat + note.length - colOffset) * (double)colWidth);
            int x0 = x() + xLeft;
            if (x0 + (xRight - xLeft) < x() || x0 > x() + w()) continue;
            drawSelectionOutline(x0, y() + rowY((int)note.row), xRight - xLeft,
                                 rowH((int)note.row));
        }
    }
    drawStamp();
    drawBand();

    fl_pop_clip();
}

SongGrid::~SongGrid()
{
    swapObserver(timeline, nullptr, this);
}

void SongGrid::setTimeline(ObservableSong* tl)
{
    // A ghost describes rows and lanes of the song being replaced.
    endStamp();
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
    if (!timeline) return false;
    int absRow = visualRow + rowOffset;
    const auto& ro = timeline->get().rowOrder;
    // Only lane rows hold pattern instances. An automation lane has no
    // destination lane for movePattern to find, so a block dropped on one is
    // discarded — and, with no notify() to trigger a rebuild, the preview stays
    // stranded on the param row. Instrument header rows are not lanes either.
    return absRow < 0 || absRow >= (int)ro.size() || ro[absRow].kind != RowKind::Lane;
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

// ---------------------------------------------------------------------------
// Multi-selection
//
// The song grid is the one editor where automation belongs in the selection:
// its param lanes are rows of this same widget, so a band drawn across the
// arrangement naturally crosses them. (In the pattern editors automation lives
// in a separate widget below the note grid, so the question never arises.)
//
// Instances are placed in BARS, and a song-level param point's `beat` is in the
// same bar units, so one horizontal delta applies to both.
// ---------------------------------------------------------------------------

std::unordered_set<int> SongGrid::liveItemIds() const
{
    std::unordered_set<int> ids;
    if (!timeline) return ids;
    for (const auto& t : timeline->get().tracks)
        for (const auto& l : t.lanes)
            for (const auto& p : l.patterns) ids.insert(p.id);
    for (const auto& lane : timeline->get().paramLanes)
        for (const auto& pt : lane.points) ids.insert(pt.id);
    return ids;
}

void SongGrid::selectAll()
{
    selection.clear();
    if (!timeline) return;
    for (const auto& t : timeline->get().tracks)
        for (const auto& l : t.lanes)
            for (const auto& p : l.patterns) selection.add(p.id);
    // Anchors are pinned to beat 0 and cannot be moved or deleted, so putting
    // them in the selection would only produce a no-op the user can see.
    for (const auto& lane : timeline->get().paramLanes)
        for (const auto& pt : lane.points)
            if (!pt.anchor) selection.add(pt.id);
}

void SongGrid::deleteSelection()
{
    if (!timeline || selection.empty()) return;
    std::vector<int> instances, points;
    for (const auto& t : timeline->get().tracks)
        for (const auto& l : t.lanes)
            for (const auto& p : l.patterns)
                if (selection.contains(p.id)) instances.push_back(p.id);
    for (const auto& lane : timeline->get().paramLanes)
        for (const auto& pt : lane.points)
            if (!pt.anchor && selection.contains(pt.id)) points.push_back(pt.id);
    selection.clear();

    ObservableSong::Batch batch(timeline);
    for (int id : instances) timeline->removePattern(id);
    for (int id : points)    timeline->removeParamPoint(id);
}

int SongGrid::absRowForLane(int laneId) const
{
    if (!timeline) return -1;
    const auto& tl = timeline->get();
    // A stacked track draws every one of its lanes on its first lane's row, and
    // that is the only one of them with a RowRef.
    int rowLaneId = laneId;
    for (const auto& t : tl.tracks) {
        bool mine = false;
        for (const auto& l : t.lanes)
            if (l.id == laneId) { mine = true; break; }
        if (!mine) continue;
        if (t.stackedLanes && !t.lanes.empty()) rowLaneId = t.lanes[0].id;
        break;
    }
    for (int i = 0; i < (int)tl.rowOrder.size(); i++)
        if (tl.rowOrder[i].kind == RowKind::Lane && tl.rowOrder[i].id == rowLaneId) return i;
    return -1;
}

bool SongGrid::collectLandings(float dBeat, int dRow, std::vector<Landing>& out) const
{
    out.clear();
    if (!timeline) return false;
    const auto& tl = timeline->get();
    for (int ti = 0; ti < (int)tl.tracks.size(); ti++) {
        const auto& t = tl.tracks[ti];
        for (const auto& l : t.lanes) {
            int srcRow = absRowForLane(l.id);
            if (srcRow < 0) continue;
            for (const auto& p : l.patterns) {
                if (!selection.contains(p.id)) continue;
                int laneId, patId;
                destLaneForAbsRow(srcRow + dRow, laneId, patId);
                if (laneId < 0) return false;
                // Same rule a single block's drag follows: a move that stays
                // inside a stacked track keeps its own lane, because all of
                // that track's lanes share the one row and the row names only
                // the first of them.
                if (t.stackedLanes && timeline->trackIndexForLaneId(laneId) == ti)
                    laneId = l.id;
                out.push_back({p.id, laneId, p.startBar + dBeat, p.length});
            }
        }
    }
    return true;
}

void SongGrid::groupDragLimits(float& minDBeat, float& maxDBeat,
                               int& minDRow, int& maxDRow) const
{
    minDBeat = maxDBeat = 0.0f;
    minDRow  = maxDRow  = 0;
    if (!timeline) return;

    bool first = true;
    auto span = [&](float lo, float hi) {
        if (first) { minDBeat = lo; maxDBeat = hi; first = false; }
        else { minDBeat = std::max(minDBeat, lo); maxDBeat = std::min(maxDBeat, hi); }
    };
    for (const auto& t : timeline->get().tracks)
        for (const auto& l : t.lanes)
            for (const auto& p : l.patterns)
                if (selection.contains(p.id))
                    span(-p.startBar, (float)numCols - (p.startBar + p.length));
    // A dot may not pass a neighbour that is staying put, so every selected dot
    // bounds the whole group: the drag stops against the neighbour instead of
    // running past it and being refused at the end. Selected dots keep their
    // spacing and so can never cross each other — only unselected ones count.
    // Points are held in beat order, and beat 0 belongs to the lane's anchor,
    // which is never selectable, so it is always the left wall of the first dot.
    for (const auto& lane : timeline->get().paramLanes) {
        const auto& pts = lane.points;
        for (int i = 0; i < (int)pts.size(); i++) {
            if (pts[i].anchor || !selection.contains(pts[i].id)) continue;
            float lo = -pts[i].beat, hi = (float)numCols - pts[i].beat;
            for (int j = i - 1; j >= 0; j--)
                if (!selection.contains(pts[j].id)) { lo = pts[j].beat - pts[i].beat; break; }
            for (int j = i + 1; j < (int)pts.size(); j++)
                if (!selection.contains(pts[j].id)) { hi = std::min(hi, pts[j].beat - pts[i].beat); break; }
            // Landing on beat 0 is not allowed — that slot is the anchor's — so
            // a dot walled in by the anchor stops one snap step short of it.
            if (snap > 0.0f)
                lo = std::max(lo, std::min(snap, pts[i].beat) - pts[i].beat);
            span(lo, hi);
        }
    }

    // Vertical. Only the instances have a row to travel along: a dot's vertical
    // position is its value, not a row, so a selection that changes rows leaves
    // its dots on the lanes they came from.
    //
    // A drag anchored on a dot stays horizontal altogether. The primary is what
    // the row delta is measured from, and a dot moving up and down its lane
    // would hand the blocks a number that means nothing.
    if (!groupPrimaryInNotes) return;
    bool firstRow = true;
    for (const auto& t : timeline->get().tracks)
        for (const auto& l : t.lanes) {
            bool anySelected = false;
            for (const auto& p : l.patterns)
                if (selection.contains(p.id)) { anySelected = true; break; }
            if (!anySelected) continue;
            int srcRow = absRowForLane(l.id);
            if (srcRow < 0) continue;
            // Bounded by the rows on screen, as a single block's drag is: the
            // view does not follow a drag vertically, so travel past the last
            // visible row would only make the selection disappear. Which of the
            // rows in between will actually take a block is left to
            // groupRowsRejected, which steps over the ones that will not.
            int vr = srcRow - rowOffset;
            int rlo = -vr, rhi = numRows - 1 - vr;
            if (firstRow) { minDRow = rlo; maxDRow = rhi; firstRow = false; }
            else { minDRow = std::max(minDRow, rlo); maxDRow = std::min(maxDRow, rhi); }
        }
}

// The base version works off `notes`, which holds only the rows on screen. Here
// the whole model is to hand, so a selection reaching past the viewport is
// judged on all of it.
bool SongGrid::groupRowsRejected(int dRow) const
{
    std::vector<Landing> landings;
    return !collectLandings(0.0f, dRow, landings);
}

bool SongGrid::groupMoveBlocked(float dBeat, int dRow) const
{
    if (!timeline) return false;

    std::vector<Landing> landings;
    // A row that cannot hold a block refuses the whole move — this is what stops
    // a selection being dragged onto an automation lane or a header row.
    if (!collectLandings(dBeat, dRow, landings)) return true;

    // An instance may only collide with instances in the lane it LANDS in, so
    // the test is per-lane rather than against every block on screen.
    for (const auto& d : landings)
        for (const auto& t : timeline->get().tracks)
            for (const auto& l : t.lanes) {
                if (l.id != d.laneId) continue;
                for (const auto& q : l.patterns) {
                    // A selected block sitting in this lane is leaving it by the
                    // same delta, so it is not in the way. Two selected blocks
                    // landing on each other is caught below.
                    if (selection.contains(q.id)) continue;
                    if (beatsOverlap(d.startBar, d.length, q.startBar, q.length)) return true;
                }
            }

    // Selected blocks keep their relative geometry and so cannot newly collide
    // with each other — except across lanes, which a row change makes possible:
    // a stacked track shows all of its lanes on one row, and two blocks from
    // that row are aimed at the same destination lane.
    for (size_t i = 0; i < landings.size(); i++)
        for (size_t j = i + 1; j < landings.size(); j++)
            if (landings[i].laneId == landings[j].laneId &&
                beatsOverlap(landings[i].startBar, landings[i].length,
                             landings[j].startBar, landings[j].length))
                return true;

    // Dots crossing their neighbours is handled in groupDragLimits, which stops
    // the drag at the neighbour rather than letting it run on and then refusing
    // the whole move; moveParamPoint would clamp them into a heap otherwise.
    return false;
}

void SongGrid::onCommitGroupMove(float dBeat, int dRow)
{
    if (!timeline || (dBeat == 0.0f && dRow == 0)) return;

    // Resolve every destination against the untouched model first: movePattern
    // rewrites the lanes as it goes.
    std::vector<Landing> landings;
    if (!collectLandings(dBeat, dRow, landings)) return;

    struct PointMove { int id; float beat; int value; };
    std::vector<PointMove> pointMoves;
    // Dots travel along the beat axis only; the row delta is not theirs.
    for (const auto& lane : timeline->get().paramLanes)
        for (const auto& pt : lane.points)
            if (!pt.anchor && selection.contains(pt.id))
                pointMoves.push_back({pt.id, pt.beat + dBeat, pt.value});

    // moveParamPoint clamps each point between its immediate neighbours, so
    // applying a group move one point at a time fights itself unless the points
    // vacate in the right order: rightmost first when moving right, leftmost
    // first when moving left.
    std::sort(pointMoves.begin(), pointMoves.end(),
              [dBeat](const PointMove& a, const PointMove& b) {
                  return dBeat > 0.0f ? a.beat > b.beat : a.beat < b.beat;
              });

    ObservableSong::Batch batch(timeline);
    // A block that changes lanes adopts that lane's pattern — movePattern's own
    // rule, since a block plays the pattern of the lane it sits in.
    for (const auto& d : landings)  timeline->movePattern(d.instId, d.laneId, d.startBar);
    for (const auto& m : pointMoves) timeline->moveParamPoint(m.id, m.beat, m.value);
}

// Add every automation dot whose centre falls inside the band. Dots are points,
// so both axes reduce to plain containment — there is no row-centre rule to
// apply, and no extent to overlap.
void SongGrid::addBandHitExtras()
{
    if (!timeline) return;
    const int dotR       = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    if (totalRange <= 0) return;

    for (int li = 0; li < (int)localParamLanes.size(); li++) {
        const auto& lane = localParamLanes[li];
        int vr = visualRowForLaneId(lane.id);
        if (vr < 0 || vr >= numRows) continue;
        const int rowTop = rowY(vr);
        const int maxVal = laneMaxValue(lane.type);
        for (const auto& pt : lane.points) {
            if (pt.anchor) continue;
            int dotX = (int)((pt.beat - colOffset) * colWidth);
            int dotY = rowTop + dotR + (int)((maxVal - pt.value) * totalRange / (float)maxVal);
            if (selection.bandContainsPoint(dotX, dotY)) selection.add(pt.id);
        }
    }
}

// Dots are drawn from localParamLanes, a copy of the model, so a group drag
// previews by shifting the selected ones there — movingGroup only knows how to
// move `notes`. The original beat comes from the model rather than a snapshot
// taken at the start of the drag: nothing edits the timeline while a drag is in
// progress, so the model IS the original, and the preview cannot drift.
void SongGrid::previewGroupExtras(float dBeat)
{
    if (!timeline) return;
    for (const auto& lane : timeline->get().paramLanes) {
        auto it = std::find_if(localParamLanes.begin(), localParamLanes.end(),
                               [&](const ParamLaneLocal& l) { return l.id == lane.id; });
        if (it == localParamLanes.end()) continue;
        for (const auto& src : lane.points) {
            if (src.anchor || !selection.contains(src.id)) continue;
            for (auto& pt : it->points)
                if (pt.id == src.id) { pt.beat = std::max(0.0f, src.beat + dBeat); break; }
        }
    }
}

void SongGrid::drawParamSelection(int laneIdx, int rowYPx) const
{
    const auto& lane = localParamLanes[laneIdx];
    const int dotR       = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    if (totalRange <= 0) return;
    const int maxVal = laneMaxValue(lane.type);
    const int r      = dotR + 2;
    fl_color(selectionColor);
    for (const auto& pt : lane.points) {
        if (!selection.contains(pt.id)) continue;
        int dotX = x() + (int)((pt.beat - colOffset) * colWidth);
        if (dotX + r < x() || dotX - r > x() + w()) continue;
        int dotY = rowYPx + dotR + (int)((maxVal - pt.value) * totalRange / (float)maxVal);
        fl_line_style(FL_SOLID, 2);
        fl_arc(dotX - r, dotY - r, 2 * r, 2 * r, 0, 360);
        fl_line_style(0);
    }
}

// ---------------------------------------------------------------------------
// Copy-and-place
//
// "Copy selection" does not paste anywhere by itself: it lifts the selected
// instances into a ghost that follows the cursor, and the click that follows
// drops the copy where the ghost stands. That keeps the destination an explicit
// choice rather than a fixed offset. Escape cancels instead.
//
// Placing ends the gesture but leaves the original selection alone, so copying
// the same blocks somewhere else again is just another right-click.
//
// The ghost is state of its own rather than a GridState variant: it is live
// with no button held, which no drag state is.
// ---------------------------------------------------------------------------

void SongGrid::destLaneForAbsRow(int absRow, int& laneId, int& patternId) const
{
    laneId = -1; patternId = 0;
    if (!timeline) return;
    const auto& ro = timeline->get().rowOrder;
    // Only lane rows take instances — the same rule isRowBlocked applies to a
    // drag, which is what keeps copies out of automation lanes and instrument
    // header rows.
    if (absRow < 0 || absRow >= (int)ro.size() || ro[absRow].kind != RowKind::Lane) return;
    laneId = ro[absRow].id;
    for (const auto& t : timeline->get().tracks)
        for (const auto& l : t.lanes)
            if (l.id == laneId) { patternId = l.patternId; return; }
}

void SongGrid::beginStamp()
{
    stamp.clear();
    stampDBar = stampBaseDBar = 0.0f;
    stampDRow = stampBaseDRow = 0;
    stampAnchored = false;
    if (!timeline || selection.empty()) return;

    bool  first = true;
    float lo = 0.0f, hi = 0.0f;
    for (const auto& t : timeline->get().tracks) {
        for (const auto& l : t.lanes) {
            int absRow = absRowForLane(l.id);
            if (absRow < 0) continue;
            for (const auto& p : l.patterns) {
                if (!selection.contains(p.id)) continue;
                stamp.push_back({absRow, p.startBar, p.length, p.startOffset});
                // Same intersection of per-item ranges as groupDragLimits: the
                // ghost stops when its first member would leave the grid, so
                // the shape never gets squashed against the edge.
                float ilo = -p.startBar, ihi = (float)numCols - (p.startBar + p.length);
                if (first) { lo = ilo; hi = ihi; first = false; }
                else       { lo = std::max(lo, ilo); hi = std::min(hi, ihi); }
            }
        }
    }
    // Selected automation dots are deliberately left out: a dot copied onto a
    // different lane has no meaning, and the lane it came from already has one
    // at that beat.
    if (stamp.empty()) return;

    includeZero(lo, hi);
    stampMinDBar = lo;
    stampMaxDBar = hi;
    redraw();
}

// Drops the ghost, whether the copy was placed or abandoned. The selection is
// deliberately untouched: the blocks that were copied stay selected either way.
void SongGrid::endStamp()
{
    if (stamp.empty()) return;
    stamp.clear();
    stampAnchored = false;
    if (window()) window()->cursor(FL_CURSOR_DEFAULT);
    redraw();
}

bool SongGrid::cancelPlacement()
{
    if (stamp.empty()) return false;
    endStamp();
    return true;
}

void SongGrid::updateStamp()
{
    if (stamp.empty()) return;
    const int ex = Fl::event_x() - x();
    const int ey = Fl::event_y() - y();

    if (!stampAnchored) {
        stampOriginX  = ex;
        stampOriginY  = ey;
        stampBaseDBar = stampDBar;
        stampBaseDRow = stampDRow;
        stampAnchored = true;
    }

    // Blocks snap to the beat of whatever time signature is in force where the
    // ghost sits, exactly as dragging one does.
    if (timeline) {
        float rawBar = (float)ex / (float)colWidth + colOffset;
        int   bpb, dummy;
        timeline->timeSigAt((int)std::max(0.0f, rawBar), bpb, dummy);
        snap = 1.0f / bpb;
    }

    float dBar = stampBaseDBar + (float)(ex - stampOriginX) / (float)colWidth;
    if (snap > 0.0f) dBar = std::round(dBar / snap) * snap;
    dBar = std::clamp(dBar, stampMinDBar, stampMaxDBar);
    if (snap > 0.0f) {
        // Re-snap after clamping: the limit itself is rarely on a grid line.
        float snapped = std::round(dBar / snap) * snap;
        if (snapped >= stampMinDBar && snapped <= stampMaxDBar) dBar = snapped;
    }
    stampDBar = dBar;

    // Rows that cannot hold a block are stepped over rather than drawn on, so
    // the ghost stays strictly in the pattern lanes: an automation lane or an
    // instrument header under the cursor holds the last row delta that worked.
    // Zero always works — that is where the blocks were copied from.
    int dRow = stampBaseDRow + rowAtPixelY(ey) - rowAtPixelY(stampOriginY);
    if (dRow != stampDRow && !stampRowsUsable(dRow)) dRow = stampDRow;
    stampDRow = dRow;

    stampBlocked = stampIsBlocked();
    if (window()) {
        if (stampBlocked) window()->cursor(forbiddenCursorImage(), 11, 11);
        else              window()->cursor(FL_CURSOR_HAND);
    }
    redraw();
}

bool SongGrid::stampRowsUsable(int dRow) const
{
    if (!timeline) return false;
    for (const auto& s : stamp) {
        // On screen: the view does not follow the ghost vertically, so a row
        // past the last visible one would only make the copy invisible.
        int vr = s.srcAbsRow + dRow - rowOffset;
        if (vr < 0 || vr >= numRows) return false;
        int laneId, patId;
        destLaneForAbsRow(s.srcAbsRow + dRow, laneId, patId);
        if (laneId < 0 || patId <= 0) return false;
    }
    return true;
}

bool SongGrid::stampIsBlocked() const
{
    if (!timeline || stamp.empty()) return true;

    struct Dest { int laneId; float startBar, length; };
    std::vector<Dest> dests;
    dests.reserve(stamp.size());
    for (const auto& s : stamp) {
        int laneId, patId;
        destLaneForAbsRow(s.srcAbsRow + stampDRow, laneId, patId);
        if (laneId < 0 || patId <= 0) return true;
        float start = s.startBar + stampDBar;
        if (start < 0.0f || start + s.length > (float)numCols) return true;
        dests.push_back({laneId, start, s.length});
    }

    // Against what is already in the destination lane. Nothing is vacating —
    // unlike a group move, the originals stay put — so every existing instance
    // counts, including the selected ones.
    for (const auto& d : dests)
        for (const auto& t : timeline->get().tracks)
            for (const auto& l : t.lanes) {
                if (l.id != d.laneId) continue;
                for (const auto& q : l.patterns)
                    if (beatsOverlap(d.startBar, d.length, q.startBar, q.length)) return true;
            }

    // And against each other: a stacked track collapses several lanes onto one
    // row, so two copies from that row are aimed at the same destination lane
    // and can collide even though their sources did not.
    for (size_t i = 0; i < dests.size(); i++)
        for (size_t j = i + 1; j < dests.size(); j++)
            if (dests[i].laneId == dests[j].laneId &&
                beatsOverlap(dests[i].startBar, dests[i].length,
                             dests[j].startBar, dests[j].length))
                return true;

    return false;
}

void SongGrid::commitStamp()
{
    if (!timeline || stamp.empty()) return;

    struct Place { int laneId, patternId; float startBar, length, startOffset; };
    std::vector<Place> places;
    places.reserve(stamp.size());
    for (const auto& s : stamp) {
        int laneId, patId;
        destLaneForAbsRow(s.srcAbsRow + stampDRow, laneId, patId);
        if (laneId < 0 || patId <= 0) return;
        // The copy takes the destination lane's pattern, because a block plays
        // the pattern of the lane it sits in — the same rule movePattern uses.
        places.push_back({laneId, patId, s.startBar + stampDBar, s.length, s.startOffset});
    }

    ObservableSong::Batch batch(timeline);   // one notification, one undo entry
    for (const auto& p : places)
        timeline->placePattern(p.laneId, p.patternId, p.startBar, p.length, p.startOffset);
}

void SongGrid::drawStamp() const
{
    if (stamp.empty()) return;
    const int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
    for (const auto& s : stamp) {
        int vr = s.srcAbsRow + stampDRow - rowOffset;
        if (vr < 0 || vr >= numRows) continue;
        // Round both edges independently, as the block pass does, so a ghost
        // lines up pixel-for-pixel with the block it will become.
        int xLeft  = (int)std::lround((s.startBar + stampDBar - colOffset) * (double)colWidth);
        int xRight = (int)std::lround((s.startBar + stampDBar + s.length - colOffset) * (double)colWidth);
        int x0     = x() + xLeft;
        int width  = xRight - xLeft;
        if (width <= 0 || x0 + width < x() || x0 > x() + gridRight) continue;
        int y0 = y() + rowY(vr);
        int rh = rowH(vr);

        // Ghostly rather than solid: a wash of the block colour leaves the grid
        // and any block underneath readable, which is the whole point of
        // showing where the copy would land before committing to it. Same
        // stretched-single-pixel trick as the rubber band — FLTK has no
        // alpha-aware rectangle.
        if (fl_can_do_alpha_blending()) {
            static const uchar pixel[4] = {
                uchar(kBlockFill >> 24), uchar(kBlockFill >> 16),
                uchar(kBlockFill >> 8),  kStampFillAlpha };
            static Fl_RGB_Image wash(pixel, 1, 1, 4);
            wash.scale(width, rh - 1, 0, 1);
            wash.draw(x0, y0 + 1);
        } else {
            fl_rectf(x0, y0 + 1, width, rh - 1, fl_color_average(kBlockFill, bgColor, 0.25f));
        }
        fl_color(stampBlocked ? kStampBlocked : selectionColor);
        fl_line_style(FL_SOLID, 2);
        fl_rect(x0 + 1, y0 + 2, std::max(2, width - 2), rh - 3);
        fl_line_style(0);
    }
}

int SongGrid::handleStampEvent(int event)
{
    switch (event) {
        case FL_ENTER:
        case FL_MOVE:
            updateStamp();
            return 1;

        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                updateStamp();             // the click may be the first event we see
                if (!stampBlocked) {
                    commitStamp();
                    // One click, one copy: the ghost goes away and the items it
                    // was copied from stay selected, so the same selection can
                    // simply be copied again.
                    endStamp();
                    // The matching FL_RELEASE now finds no ghost and reaches
                    // Grid, whose idle release creates or removes a block. This
                    // click has already been spent.
                    creationForbidden = true;
                }
            } else {
                endStamp();
            }
            return 1;

        case FL_LEAVE:
            // Keep the mode alive; re-anchor so the ghost picks up from where it
            // stands when the cursor comes back rather than springing about.
            stampAnchored = false;
            if (window()) window()->cursor(FL_CURSOR_DEFAULT);
            return 1;

        case FL_HIDE:
            // Switching to a pattern editor abandons the copy: leaving a ghost
            // parked on a grid the user has walked away from would ambush them
            // when they came back.
            endStamp();
            return Grid::handle(event);

        case FL_MOUSEWHEEL:
            // Let the view scroll, but the pixel anchor is meaningless once the
            // rows have moved under it.
            stampAnchored = false;
            return Grid::handle(event);

        case FL_KEYBOARD:
        case FL_SHORTCUT: {
            // Escape normally arrives via AppWindow (these grids never take
            // focus); handled here too so the ghost still goes away if the key
            // ever reaches the grid directly.
            int key = Fl::event_key();
            if (key == FL_Escape) { endStamp(); return 1; }
            // No hover-delete under a ghost. Every other key is someone else's:
            // this is a broadcast, so claiming it would steal it from them.
            if (key == FL_Delete || key == FL_BackSpace) return 1;
            return Grid::handle(event);
        }

        case FL_DRAG:
        case FL_RELEASE:
            return 1;   // nothing may be dragged, created or deleted underneath

        default:
            return Grid::handle(event);
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
            if (!pt.anchor && selection.contains(pt.id)) {
                // Grabbing a dot that is part of a selection drags the whole of
                // it, instances included — the same rule as grabbing a selected
                // pattern block. Grid runs the drag from here on; the dot is the
                // primary, so the grab offset is measured from its centre.
                int dotX = (int)((pt.beat - colOffset) * colWidth);
                int vr   = visualRowForLaneId(localParamLanes[laneIdx].id);
                beginGroupDrag(Point{vr, pt.beat},
                               (float)(Fl::event_x() - x() - dotX), 0.0f);
                if (window()) window()->cursor(FL_CURSOR_HAND);
                return 1;
            }
            // Grabbing anything else drops the selection and moves that dot
            // alone, as it does for a pattern block.
            if (!selection.empty()) { selection.clear(); redraw(); }
            paramState = ParamDragState{laneIdx, ptIdx, pt.beat, pt.value};
            if (window()) window()->cursor(FL_CURSOR_HAND);
        } else if (!selection.empty()) {
            // A plain click on empty lane space with a selection active only
            // dismisses it. Adding a dot as well would make the selection
            // impossible to drop without editing something.
            selection.clear();
            paramState = ParamIdle{};
            redraw();
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
            int ex        = (int)dragX();
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
            updateEdgeScroll();
            redraw();
        }
        return 1;
    }

    case FL_RELEASE: {
        stopEdgeScroll();
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

void SongGrid::setScroll(int rowOff, int pxOff)
{
    rowOffset   = rowOff;
    pixelOffset = pxOff;
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
    // A group drag previews the dots in localParamLanes too, so rebuilding it
    // mid-drag would wipe the preview just as it would for the notes.
    if (!isActiveDrag() &&
        !std::holds_alternative<ParamDragState>(paramState) &&
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
    // Right-clicking a member of a multi-selection is a different question from
    // right-clicking one block, so it gets its own menu rather than a mode of
    // the instance one. Right-clicking anything else falls through unchanged.
    if (selectionPopup && !selection.empty() && selection.contains(notes[idx].id)) {
        selectionPopup->open(this,
            [this]() { beginStamp(); },
            [this]() { deleteSelectedItems(); redraw(); });
        return;
    }
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

    for (int i = 0; i < (int)notes.size(); ++i) {
        const Note& b = notes[i];
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
    if (timeline) {
        const Pattern* pat = timeline->patternForInstance(notes[noteIdx].id);
        dragBeatsPerBar = timeline->patternBeatsPerBar((int)notes[noteIdx].beat,
                                                       pat ? pat->id : 0);
    }
    float startOffset = 0.0f;
    if (const PatternInstance* inst = timeline->instanceById(notes[noteIdx].id))
        startOffset = inst->startOffset;
    tickBarPos = notes[noteIdx].beat - startOffset / dragBeatsPerBar;
}

void SongGrid::moving(StateDragMove& s)
{
    if (timeline) {
        float ex      = dragX();
        float rawBeat = (ex - s.grabX) / (float)colWidth + colOffset;
        int   bpb, dummy;
        timeline->timeSigAt((int)std::max(0.0f, rawBeat), bpb, dummy);
        snap = 1.0f / bpb;
    }
    Grid::moving(s);
}

bool SongGrid::isItemDrag() const
{
    return Grid::isItemDrag() || std::holds_alternative<ParamDragState>(paramState);
}

void SongGrid::reapplyDrag()
{
    if (std::holds_alternative<ParamDragState>(paramState)) handleParamEvent(FL_DRAG);
    else                                                    Grid::reapplyDrag();
}

void SongGrid::resizing(StateDragResize& s)
{
    if (timeline) {
        float ex      = dragX();
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
    if (selectionPopup && selectionPopup->visible()) return 0;

    // A ghost waiting to be placed owns the grid outright: no hovering,
    // dragging or creating happens underneath it.
    if (!stamp.empty()) return handleStampEvent(event);

    // Active param interaction takes priority over everything
    if (!std::holds_alternative<ParamIdle>(paramState))
        return handleParamEvent(event);

    // A selection gesture spans instance rows and automation rows alike, so it
    // has to be decided before the by-row routing below — otherwise a band that
    // happened to start over a param lane would be swallowed by the dot editor.
    if (isActiveDrag())
        return Grid::handle(event);

    // Ctrl-click over an automation lane toggles the dot under the cursor.
    // Grid's own ctrl-click reads the hovered item out of `notes`, which holds
    // pattern instances only, so the dot has to be resolved here. Shift still
    // goes to Grid: a band sweeps both kinds of row at once.
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE &&
        (Fl::event_state() & FL_COMMAND) && !(Fl::event_state() & FL_SHIFT) &&
        !localParamLanes.empty() && timeline) {
        int laneIdx = laneIdxForAbsRow(rowAtPixelY(Fl::event_y() - y()) + rowOffset);
        if (laneIdx >= 0) {
            int ptIdx = findParamPointAtCursor(laneIdx);
            // Anchors are pinned to beat 0 and cannot move or be deleted, so
            // they stay out of selections — see selectAll().
            if (ptIdx >= 0 && !localParamLanes[laneIdx].points[ptIdx].anchor) {
                selection.toggle(localParamLanes[laneIdx].points[ptIdx].id);
                redraw();
            }
            paramState = ParamIdle{};   // and the release must not create a dot
            return 1;
        }
    }

    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE &&
        (Fl::event_state() & (FL_SHIFT | FL_COMMAND)))
        return Grid::handle(event);

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
