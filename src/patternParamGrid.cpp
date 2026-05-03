#include "patternParamGrid.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>

static constexpr Fl_Color kParamRowBg  = 0xF0F4FF00;
static constexpr Fl_Color kParamLine   = 0x8888CC00;
static constexpr Fl_Color kParamDotFill = 0x5555EE00;
static constexpr Fl_Color kParamDotRim  = 0x1111EE00;
static constexpr Fl_Color kRowSep      = 0xCCCCEE00;

// ── PatternParamLabels ────────────────────────────────────────────────────────

void PatternParamLabels::draw()
{
    static constexpr Fl_Color kBg     = 0x1A2B3A00;
    static constexpr Fl_Color kBorder = 0x2D374800;
    static constexpr Fl_Color kText   = 0xA0AEC000;
    static constexpr Fl_Color kRowBg  = 0x1E3048;

    fl_color(kBg);
    fl_rectf(x(), y(), w(), h());
    fl_color(kBorder);
    fl_rectf(x() + w() - 1, y(), 1, h());

    const Pattern* pat = nullptr;
    if (pattern) {
        for (const auto& p : pattern->get().patterns)
            if (p.id == patternId) { pat = &p; break; }
    }

    fl_font(FL_HELVETICA, 10);
    const int visRows = h() / kParamRowH;
    for (int r = 0; r < visRows; r++) {
        int rowY = y() + r * kParamRowH;
        fl_color(0x1C2D3E00);
        fl_rectf(x(), rowY, w() - 1, kParamRowH);
        fl_color(kRowSep);
        fl_line(x(), rowY, x() + w() - 1, rowY);
        if (pat) {
            int li = r + laneOffset;
            if (li < (int)pat->paramLanes.size()) {
                fl_color(kText);
                fl_draw(pat->paramLanes[li].type.c_str(),
                        x() + 4, rowY, w() - 8, kParamRowH,
                        FL_ALIGN_LEFT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            }
        }
    }
    fl_color(kRowSep);
    fl_line(x(), y() + h() - 1, x() + w() - 1, y() + h() - 1);
}

int PatternParamLabels::handle(int event)
{
    switch (event) {
    case FL_ENTER:
        return 1;
    case FL_PUSH:
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            int vr = (Fl::event_y() - y()) / kParamRowH;
            int laneIdx = vr + laneOffset;
            int laneId = -1;
            if (pattern) {
                for (const auto& p : pattern->get().patterns) {
                    if (p.id != patternId) continue;
                    if (laneIdx < (int)p.paramLanes.size())
                        laneId = p.paramLanes[laneIdx].id;
                    break;
                }
            }
            if (onRightClick) onRightClick(laneId);
            return 1;
        }
        return 1;
    default:
        return Fl_Widget::handle(event);
    }
}

// ── PatternParamGrid ──────────────────────────────────────────────────────────

void PatternParamGrid::rebuildLanes()
{
    localLanes.clear();
    if (!pattern || patternId < 0) return;
    for (const auto& p : pattern->get().patterns) {
        if (p.id != patternId) continue;
        for (const auto& lane : p.paramLanes) {
            ParamLaneLocal local;
            local.id   = lane.id;
            local.type = lane.type;
            for (const auto& pt : lane.points)
                local.points.push_back({pt.id, pt.beat, pt.value, pt.anchor});
            localLanes.push_back(std::move(local));
        }
        break;
    }
}

void PatternParamGrid::setPattern(ObservablePattern* tl, int patId)
{
    pattern  = tl;
    patternId = patId;
    paramState = ParamIdle{};
    rebuildLanes();
    redraw();
}

void PatternParamGrid::update(ObservablePattern* tl, int patId)
{
    pattern  = tl;
    patternId = patId;
    if (!std::holds_alternative<ParamDragState>(paramState) &&
        !std::holds_alternative<ParamVirtualDrag>(paramState)) {
        rebuildLanes();
    }
    redraw();
}

void PatternParamGrid::draw()
{
    fl_push_clip(x(), y(), w() + 1, h() + 1);

    int gridRight = std::min(w(), (numCols_ - colOffset_) * colWidth_);
    const int visRows = h() / kParamRowH;

    // Row backgrounds
    for (int r = 0; r < visRows; r++) {
        fl_color(kParamRowBg);
        fl_rectf(x(), y() + r * kParamRowH, gridRight, kParamRowH);
    }

    // Horizontal separators
    for (int r = 0; r <= visRows; r++) {
        fl_color(kRowSep);
        fl_line(x(), y() + r * kParamRowH, x() + gridRight, y() + r * kParamRowH);
    }

    // Vertical column lines matching patternGrid colors
    int timeSigTop = 4;
    if (pattern) {
        for (const auto& p : pattern->get().patterns)
            if (p.id == patternId) { timeSigTop = p.timeSigTop; break; }
    }
    int endCol = colOffset_ + w() / colWidth_ + 2;
    for (int i = colOffset_; i <= std::min(endCol, numCols_); i++) {
        int x0 = x() + (i - colOffset_) * colWidth_;
        bool isBar = timeSigTop > 0 && i % timeSigTop == 0;
        fl_color(isBar ? (Fl_Color)0x00660000 : (Fl_Color)0x00EE0000);
        fl_line(x0, y(), x0, y() + h());
    }

    // Param rubberbands
    for (int r = 0; r < visRows; r++) {
        int li = r + laneOffset;
        if (li >= (int)localLanes.size()) break;
        drawParamRow(li, y() + r * kParamRowH, gridRight);
    }

    fl_pop_clip();
}

void PatternParamGrid::drawParamRow(int laneIdx, int rowY, int gridRight)
{
    const auto& lane = localLanes[laneIdx];
    const int dotR = std::max(2, kParamRowH / 9);
    const int totalRange = kParamRowH - 1 - 2 * dotR;
    if (totalRange <= 0) return;
    const int maxVal = laneMaxValue(lane.type);

    auto dotYFor = [&](int value) {
        return rowY + dotR + (int)((maxVal - value) * totalRange / (float)maxVal);
    };

    // Virtual dot (preceding off-screen point)
    {
        int predIdx = findPrecedingDotIdx(laneIdx);
        bool draw = predIdx >= 0;
        if (draw) {
            for (const auto& pt : lane.points) {
                int dotX = x() + (int)((pt.beat - colOffset_) * colWidth_);
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

    for (int i = 0; i + 1 < (int)lane.points.size(); i++) {
        const auto& a = lane.points[i];
        const auto& b = lane.points[i + 1];
        fl_line(x() + (int)((a.beat - colOffset_) * colWidth_), dotYFor(a.value),
                x() + (int)((b.beat - colOffset_) * colWidth_), dotYFor(b.value));
    }

    if (!lane.points.empty()) {
        const auto& last = lane.points.back();
        int lastX = x() + (int)((last.beat - colOffset_) * colWidth_);
        int lastY = dotYFor(last.value);
        if (lastX < x() + gridRight)
            fl_line(lastX, lastY, x() + gridRight, lastY);
    }

    for (const auto& pt : lane.points) {
        int dotX = x() + (int)((pt.beat - colOffset_) * colWidth_);
        if (dotX + dotR < x() || dotX - dotR > x() + w()) continue;
        int dotY = dotYFor(pt.value);
        fl_color(kParamDotFill);
        fl_pie(dotX - dotR, dotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
        fl_color(kParamDotRim);
        fl_arc(dotX - dotR, dotY - dotR, 2 * dotR, 2 * dotR, 0, 360);
    }
}

int PatternParamGrid::findParamPointAtCursor(int laneIdx, int rowY) const
{
    if (laneIdx < 0 || laneIdx >= (int)localLanes.size()) return -1;
    const int dotR      = std::max(2, kParamRowH / 9);
    const int totalRange = kParamRowH - 1 - 2 * dotR;
    const int hitR      = dotR + 4;

    int ex = Fl::event_x();
    int ey = Fl::event_y();
    int bestIdx  = -1;
    float bestDist = (float)(hitR + 1);

    const int maxVal = laneMaxValue(localLanes[laneIdx].type);
    for (int i = 0; i < (int)localLanes[laneIdx].points.size(); i++) {
        const auto& pt = localLanes[laneIdx].points[i];
        int dotX = x() + (int)((pt.beat - colOffset_) * colWidth_);
        int dotY = rowY + dotR + (totalRange > 0 ? (int)((maxVal - pt.value) * totalRange / (float)maxVal) : 0);
        float dx = (float)(ex - dotX);
        float dy = (float)(ey - dotY);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < (float)hitR && dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

int PatternParamGrid::findPrecedingDotIdx(int laneIdx) const
{
    if (laneIdx < 0 || laneIdx >= (int)localLanes.size()) return -1;
    const auto& pts = localLanes[laneIdx].points;
    int best = -1;
    for (int i = 0; i < (int)pts.size(); i++)
        if (pts[i].beat < (float)colOffset_) best = i;
    return best;
}

bool PatternParamGrid::canPlaceDot(int laneIdx, float beat, int excludeId) const
{
    if (beat == 0.0f) return false;
    if (laneIdx < 0 || laneIdx >= (int)localLanes.size()) return false;
    int count = 0;
    for (const auto& pt : localLanes[laneIdx].points) {
        if (pt.id == excludeId) continue;
        if (pt.beat == beat) count++;
    }
    return count < 2;
}

int PatternParamGrid::handle(int event)
{
    const int dotR       = std::max(2, kParamRowH / 9);
    const int totalRange = kParamRowH - 1 - 2 * dotR;
    const int hitR       = dotR + 4;

    int ey = Fl::event_y() - y();
    const int visRows = std::max(1, h() / kParamRowH);
    int vr = std::clamp(ey / kParamRowH, 0, visRows - 1);
    int laneIdx = vr + laneOffset;
    bool overLane = laneIdx < (int)localLanes.size();

    int gridRight = std::min(w(), (numCols_ - colOffset_) * colWidth_);

    auto dotYForRow = [&](int li, int rowY, int value) {
        int mv = laneMaxValue(li < (int)localLanes.size() ? localLanes[li].type : std::string{});
        return rowY + dotR + (totalRange > 0 ? (int)((mv - value) * totalRange / (float)mv) : 0);
    };

    auto virtualDotY = [&](int li, int predIdx) {
        int rowY = y() + (li - laneOffset) * kParamRowH;
        return dotYForRow(li, rowY, localLanes[li].points[predIdx].value);
    };

    auto isVirtualOverlapped = [&](int li) {
        for (const auto& pt : localLanes[li].points) {
            int dotX = x() + (int)((pt.beat - colOffset_) * colWidth_);
            if (std::abs(dotX - x()) < 2 * dotR) return true;
        }
        return false;
    };

    // Active drag: route everything here
    if (!std::holds_alternative<ParamIdle>(paramState)) {
        switch (event) {
        case FL_DRAG: {
            if (auto* d = std::get_if<ParamVirtualDrag>(&paramState)) {
                int maxVal  = laneMaxValue(localLanes[d->laneIdx].type);
                int laneVR  = d->laneIdx - laneOffset;
                int eyInRow = ey - laneVR * kParamRowH;
                int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
                int newVal  = totalRange > 0 ? maxVal - (int)(mapped * (float)maxVal / totalRange) : maxVal / 2;
                newVal = std::clamp(newVal, 0, maxVal);
                auto& pt = localLanes[d->laneIdx].points[d->predPtIdx];
                if (newVal != pt.value) d->moved = true;
                pt.value = newVal;
                redraw();
                return 1;
            }
            if (auto* d = std::get_if<ParamDragState>(&paramState)) {
                int ex = Fl::event_x() - x();
                bool isAnchor = localLanes[d->laneIdx].points[d->ptIdx].anchor;

                float newBeat = (float)ex / colWidth_ + colOffset_;
                if (snap_ > 0.0f) newBeat = std::round(newBeat / snap_) * snap_;
                newBeat = std::max(0.0f, std::min((float)numCols_, newBeat));
                if (isAnchor) {
                    newBeat = 0.0f;
                } else {
                    if (snap_ > 0.0f) newBeat = std::max(snap_, newBeat);
                    const auto& pts = localLanes[d->laneIdx].points;
                    float lo = pts[d->ptIdx - 1].beat;
                    float hi = (d->ptIdx + 1 < (int)pts.size()) ? pts[d->ptIdx + 1].beat : (float)numCols_;
                    newBeat = std::clamp(newBeat, lo, hi);
                }

                int maxVal  = laneMaxValue(localLanes[d->laneIdx].type);
                int laneVR  = d->laneIdx - laneOffset;
                int eyInRow = ey - laneVR * kParamRowH;
                int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
                int newValue = totalRange > 0 ? maxVal - (int)(mapped * (float)maxVal / totalRange) : maxVal / 2;
                newValue = std::clamp(newValue, 0, maxVal);

                auto& pt = localLanes[d->laneIdx].points[d->ptIdx];
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
                auto& pt = localLanes[d->laneIdx].points[d->predPtIdx];
                int ptId = pt.id; float beat = pt.beat; int value = pt.value; bool moved = d->moved;
                paramState = ParamIdle{};
                if (moved && pattern) pattern->moveParamPoint(ptId, beat, value);
                else { rebuildLanes(); redraw(); }
            } else if (auto* d = std::get_if<ParamDragState>(&paramState)) {
                auto& pt = localLanes[d->laneIdx].points[d->ptIdx];
                int ptId = pt.id; float beat = pt.beat; int value = pt.value;
                bool moved = d->moved; float origBeat = d->origBeat;
                paramState = ParamIdle{};
                if (moved && pattern) {
                    float validBeat = canPlaceDot(d->laneIdx, beat, ptId) ? beat : origBeat;
                    pattern->moveParamPoint(ptId, validBeat, value);
                } else { rebuildLanes(); redraw(); }
            } else if (auto* d = std::get_if<ParamPendingCreate>(&paramState)) {
                int li = d->laneIdx; float beat = d->beat; int value = d->value;
                paramState = ParamIdle{};
                if (pattern && li >= 0 && li < (int)localLanes.size() && canPlaceDot(li, beat))
                    pattern->addPatternParamPoint(patternId, localLanes[li].id, beat, value);
                else
                    redraw();
            } else {
                paramState = ParamIdle{};
            }
            if (window()) window()->cursor(FL_CURSOR_DEFAULT);
            return 1;
        }
        default:
            break;
        }
    }

    switch (event) {
    case FL_ENTER: return 1;

    case FL_PUSH: {
        if (!overLane) return 1;
        int ex = Fl::event_x() - x();
        if (ex >= gridRight) { paramState = ParamIdle{}; return 1; }

        int rowY = y() + vr * kParamRowH;
        int ptIdx = findParamPointAtCursor(laneIdx, rowY);

        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (ptIdx >= 0 && dotPopup) {
                auto& pt = localLanes[laneIdx].points[ptIdx];
                int ptId = pt.id; float beat = pt.beat; int val = pt.value; bool anc = pt.anchor;
                int maxVal = laneMaxValue(localLanes[laneIdx].type);
                paramState = ParamIdle{};
                dotPopup->open(Fl::event_x_root(), Fl::event_y_root(), val, anc, maxVal,
                    [this, ptId, beat](int newVal) {
                        if (pattern) pattern->moveParamPoint(ptId, beat, newVal);
                    },
                    [this, ptId, anc]() {
                        if (pattern && !anc) pattern->removeParamPoint(ptId);
                    });
            } else {
                paramState = ParamIdle{};
            }
            return 1;
        }

        if (ptIdx >= 0) {
            auto& pt = localLanes[laneIdx].points[ptIdx];
            paramState = ParamDragState{laneIdx, ptIdx, pt.beat, pt.value};
            if (window()) window()->cursor(FL_CURSOR_HAND);
        } else {
            bool hitVirtual = false;
            int predIdx = findPrecedingDotIdx(laneIdx);
            if (predIdx >= 0 && !isVirtualOverlapped(laneIdx)) {
                int vdotY = virtualDotY(laneIdx, predIdx);
                float dx = (float)(Fl::event_x() - x());
                float dy = (float)(Fl::event_y() - vdotY);
                if (std::sqrt(dx * dx + dy * dy) <= (float)hitR) {
                    paramState = ParamVirtualDrag{laneIdx, predIdx,
                                                  localLanes[laneIdx].points[predIdx].value};
                    if (window()) window()->cursor(FL_CURSOR_HAND);
                    hitVirtual = true;
                }
            }
            if (!hitVirtual) {
                int maxVal  = laneMaxValue(laneIdx < (int)localLanes.size() ? localLanes[laneIdx].type : std::string{});
                float beat  = (float)ex / colWidth_ + colOffset_;
                if (snap_ > 0.0f) beat = std::round(beat / snap_) * snap_;
                beat = std::max(0.0f, beat);
                int eyInRow = ey - vr * kParamRowH;
                int mapped  = std::clamp(eyInRow - dotR, 0, totalRange > 0 ? totalRange : 0);
                int value   = totalRange > 0 ? maxVal - (int)(mapped * (float)maxVal / totalRange) : maxVal / 2;
                paramState  = ParamPendingCreate{laneIdx, beat, std::clamp(value, 0, maxVal)};
            }
        }
        return 1;
    }

    case FL_MOVE: {
        bool useHand = false;
        if (overLane) {
            int rowY = y() + vr * kParamRowH;
            useHand = findParamPointAtCursor(laneIdx, rowY) >= 0;
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
        return Fl_Widget::handle(event);
    }
}
