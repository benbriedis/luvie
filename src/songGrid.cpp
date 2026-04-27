#include "songGrid.hpp"
#include "editor.hpp"
#include "playhead.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>

static constexpr Fl_Color kParamRowBg  = 0xF0F4FF00;
static constexpr Fl_Color kParamLine   = 0x8888CC00;
static constexpr Fl_Color kParamDotFill = 0x5555EE00;
static constexpr Fl_Color kParamDotRim  = 0x1111EE00;

SongGrid::SongGrid(int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup)
    : Grid(numRows, numCols, rowHeight, colWidth, snap, popup)
{}

void SongGrid::draw()
{
    Grid::draw();
    if (!timeline) return;

    fl_push_clip(x(), y(), w(), h());
    const int tickH = 4;
    for (const auto& note : notes) {
        if (note.pitch < 0 || note.pitch >= numRows) continue;
        int y0  = y() + (int)(note.pitch * rowHeight);
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
            fl_rectf(tickX, y0 + 1,                     2, tickH);
            fl_rectf(tickX, y0 + rowHeight - 1 - tickH, 2, tickH);
            if (intervalBars <= 0.0f) break;
            tickBar += intervalBars;
        }
    }

    // Param lane dots and rubber bands
    if (!localParamLanes.empty()) {
        int numTracks = (int)timeline->get().tracks.size();
        int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
        for (int li = 0; li < (int)localParamLanes.size(); li++) {
            int vr = li + numTracks - rowOffset;
            if (vr < 0 || vr >= numRows) continue;
            drawParamRow(li, y() + vr * rowHeight, gridRight);
        }
    }

    fl_pop_clip();
}

SongGrid::~SongGrid()
{
    swapObserver(timeline, nullptr, this);
}

void SongGrid::setTimeline(ObservableTimeline* tl)
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
        local.id = lane.id;
        for (const auto& pt : lane.points)
            local.points.push_back({pt.id, pt.beat, pt.value, pt.anchor});
        localParamLanes.push_back(std::move(local));
    }
}

Fl_Color SongGrid::rowBgColor(int visualRow) const
{
    if (localParamLanes.empty() || !timeline) return bgColor;
    int numTracks = (int)timeline->get().tracks.size();
    if (visualRow + rowOffset >= numTracks) return kParamRowBg;
    return bgColor;
}

void SongGrid::drawParamRow(int laneIdx, int rowY, int gridRight)
{
    const auto& lane = localParamLanes[laneIdx];
    const int dotR = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    if (totalRange <= 0) return;

    auto dotYFor = [&](int value) {
        return rowY + dotR + (int)((127 - value) * totalRange / 127.0f);
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
    int numTracks = (int)timeline->get().tracks.size();
    int vr = laneIdx + numTracks - rowOffset;
    if (vr < 0 || vr >= numRows) return -1;

    const int dotR      = std::max(2, rowHeight / 9);
    const int totalRange = rowHeight - 1 - 2 * dotR;
    const int hitR      = dotR + 4;

    int rowY = y() + vr * rowHeight;
    int ex   = Fl::event_x();
    int ey   = Fl::event_y();

    int   bestIdx  = -1;
    float bestDist = (float)(hitR + 1);

    for (int i = 0; i < (int)localParamLanes[laneIdx].points.size(); i++) {
        const auto& pt = localParamLanes[laneIdx].points[i];
        int dotX = x() + (int)((pt.beat - colOffset) * colWidth);
        int dotY = rowY + dotR + (totalRange > 0 ? (int)((127 - pt.value) * totalRange / 127.0f) : 0);
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
    int numTracks = timeline ? (int)timeline->get().tracks.size() : 0;

    int ey  = Fl::event_y() - y();
    int vr  = ey / rowHeight;
    int laneIdx = vr + rowOffset - numTracks;

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
        int laneVR = li + numTracks - rowOffset;
        int rowY   = y() + laneVR * rowHeight;
        int value  = localParamLanes[li].points[predIdx].value;
        return rowY + dotR + (totalRange > 0 ? (int)((127 - value) * totalRange / 127.0f) : 0);
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
                paramState = ParamIdle{};
                paramDotPopup->open(Fl::event_x_root(), Fl::event_y_root(), val, anc,
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
                float beat = (float)ex / colWidth + colOffset;
                if (snap > 0.0f) beat = std::round(beat / snap) * snap;
                beat = std::max(0.0f, beat);
                int eyInRow = ey - vr * rowHeight;
                int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
                int value   = totalRange > 0 ? 127 - (int)(mapped * 127.0f / totalRange) : 63;
                paramState  = ParamPendingCreate{laneIdx, beat, std::clamp(value, 0, 127)};
            }
        }
        return 1;
    }

    case FL_DRAG: {
        if (auto* d = std::get_if<ParamVirtualDrag>(&paramState)) {
            int laneVR  = d->laneIdx + numTracks - rowOffset;
            int eyInRow = ey - laneVR * rowHeight;
            int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
            int newVal  = totalRange > 0 ? 127 - (int)(mapped * 127.0f / totalRange) : 63;
            newVal = std::clamp(newVal, 0, 127);
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

            int laneVR   = d->laneIdx + numTracks - rowOffset;
            int eyInRow  = ey - laneVR * rowHeight;
            int mapped   = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
            int newValue = totalRange > 0 ? 127 - (int)(mapped * 127.0f / totalRange) : 63;
            newValue     = std::clamp(newValue, 0, 127);

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
        if (window()) window()->cursor(useHand ? FL_CURSOR_HAND : FL_CURSOR_DEFAULT);
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
            for (auto& p : tracks[trackFilter].patterns)
                notes.push_back({p.id, 0, p.startBar * scale, p.length * scale});
        }
        clampSelection();
        return;
    }
    std::vector<Note> all = timeline->buildNotes();
    notes.clear();
    for (auto n : all) {
        int visual = (int)n.pitch - rowOffset;
        if (visual >= 0 && visual < numRows) {
            n.pitch = (float)visual;
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
    int trackIndex = (int)notes[idx].pitch + rowOffset;
    songPopup->open(&notes, idx, this,
        makeDeleteCallback(idx),
        onOpenPattern ? std::function<void()>([this, trackIndex]() { onOpenPattern(trackIndex); })
                      : nullptr);
}

void SongGrid::onBeginDrag(int noteIdx)
{
    if (timeline) { int dummy; timeline->timeSigAt((int)notes[noteIdx].beat, dragBeatsPerBar, dummy); }
    float startOffset = 0.0f;
    if (const PatternInstance* inst = timeline->instanceById(notes[noteIdx].id))
        startOffset = inst->startOffset;
    tickBarPos = notes[noteIdx].beat - startOffset / dragBeatsPerBar;
}

void SongGrid::resizing(StateDragResize& s)
{
    Grid::resizing(s);
    if (s.side == Side::Left) {
        float newOffset = (notes[s.noteIdx].beat - tickBarPos) * dragBeatsPerBar;
        dragStartOffset = newOffset;
    }
}

void SongGrid::onCommitMove(const StateDragMove& s)
{
    if (!timeline) return;
    int id = notes[s.noteIdx].id;
    timeline->movePattern(id, (int)notes[s.noteIdx].pitch + rowOffset, notes[s.noteIdx].beat);
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

    // Route to param handler when cursor is in the param row area
    if (!localParamLanes.empty() && timeline) {
        int ey = Fl::event_y() - y();
        int numTracks = (int)timeline->get().tracks.size();
        int visTrackRows = std::max(0, std::min(numTracks - rowOffset, numRows));
        if (ey >= visTrackRows * rowHeight)
            return handleParamEvent(event);
    }

    return Grid::handle(event);
}

void SongGrid::onNoteDoubleClick(int noteIdx)
{
    if (onPatternDoubleClick)
        onPatternDoubleClick((int)notes[noteIdx].pitch + rowOffset);
}

void SongGrid::toggleNote()
{
    if (trackFilter >= 0) return;
    int   ex        = Fl::event_x() - x();
    int   ey        = Fl::event_y() - y();
    int   visualRow = ey / rowHeight;
    int   absRow    = visualRow + rowOffset;
    float col       = (float)(ex / colWidth) + colOffset;

    if (!timeline) { Grid::toggleNote(); return; }

    for (auto& n : notes) {
        if ((int)n.pitch == visualRow && n.beat == col) {
            timeline->removePattern(n.id);
            return;
        }
    }
    bool clear = std::none_of(notes.begin(), notes.end(),
        [=](const Note& n) { return (int)n.pitch == visualRow && col < n.beat + n.length && col + 1.0f > n.beat; });
    const auto& tracks = timeline->get().tracks;
    if (clear && absRow >= 0 && absRow < (int)tracks.size()) {
        int patId = tracks[absRow].patternId;
        if (patId > 0)
            timeline->placePattern(absRow, patId, col, 1.0f);
    }
}
