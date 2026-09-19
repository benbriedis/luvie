// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "sliceController.hpp"
#include "grid.hpp"          // bandColor, drawBandWash
#include "observablePattern.hpp"
#include "selection.hpp"     // includeZero
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>

void SliceController::setPattern(ObservablePattern* p, int patId)
{
    pattern = p;
    if (patId != patternId) {
        // A slice is a range of one pattern; it means nothing in the next.
        patternId = patId;
        has = sweeping = dragging = false;
        changed();
        return;
    }
    // The pattern was shortened under the slice: keep what is left of it.
    const float len = patternBeats();
    if (has && end > len) {
        if (start >= len) has = false;
        else              end = len;
        changed();
    }
}

float SliceController::patternBeats() const
{
    if (!pattern) return 0.0f;
    const Pattern* p = pattern->song()->patternById(patternId);
    return p ? p->lengthBeats : 0.0f;
}

ClipKind SliceController::kind() const
{
    if (!pattern) return ClipKind::None;
    const Pattern* p = pattern->song()->patternById(patternId);
    if (!p) return ClipKind::None;
    switch (p->type) {
        case PatternType::PIANOROLL: return ClipKind::PianorollSlice;
        case PatternType::DRUM:      return ClipKind::DrumSlice;
        default:                     return ClipKind::HarmonySlice;
    }
}

float SliceController::snapped(float beat) const
{
    if (snap > 0.0f) beat = std::round(beat / snap) * snap;
    return std::clamp(beat, 0.0f, patternBeats());
}

void SliceController::changed() const
{
    for (Fl_Widget* w : views) w->redraw();
}

SliceController::Press SliceController::press(float beat, bool alt, bool otherModifiers)
{
    if (!pattern || patternId < 0) return Press::Ignored;

    if (alt) {
        // A new sweep replaces any slice there was.
        has = dragging = false;
        sweeping = true;
        anchor = cur = snapped(beat);
        if (onSweepStart) onSweepStart();
        changed();
        return Press::Consumed;
    }

    if (has && !otherModifiers && contains(beat)) {
        // Grabbing the slice drags it. Its contents are captured now, so the
        // travel is limited by the notes it carries past its edges as well as
        // by its own ends.
        float lo, hi;
        sliceFitLimits(pattern->captureRange(patternId, start, end), patternBeats(), lo, hi);
        minD = lo - start;
        maxD = hi - start;
        includeZero(minD, maxD);
        grabBeat = beat;
        dBeat    = 0.0f;
        dragging = true;
        return Press::Consumed;
    }

    if (has) {
        // A press anywhere else lets go of the slice. A plain click does only
        // that — as it does with an item selection, so dismissing a slice never
        // also creates a note — while a Shift-band or ctrl-click goes on to
        // start the item selection it asked for.
        clear();
        return otherModifiers ? Press::Dismissed : Press::Consumed;
    }
    return Press::Ignored;
}

void SliceController::drag(float beat)
{
    if (sweeping) {
        cur = snapped(beat);
        changed();
    }
    else if (dragging) {
        float d = beat - grabBeat;
        if (snap > 0.0f) d = std::round(d / snap) * snap;
        d = std::clamp(d, minD, maxD);
        // Re-snap after clamping: the limit itself is rarely on a grid line.
        if (snap > 0.0f) {
            float s = std::round(d / snap) * snap;
            if (s >= minD && s <= maxD) d = s;
        }
        if (d != dBeat) { dBeat = d; changed(); }
    }
}

void SliceController::release()
{
    if (sweeping) {
        sweeping = false;
        start = std::min(anchor, cur);
        end   = std::max(anchor, cur);
        // An Alt-click that never became a drag leaves no slice behind.
        has = end - start > 1e-4f;
        changed();
    }
    else if (dragging) {
        dragging = false;
        const float d = dBeat;
        dBeat = 0.0f;
        if (std::abs(d) > 1e-6f && pattern &&
            pattern->moveRange(patternId, start, end, start + d)) {
            start += d;
            end   += d;
        }
        changed();
    }
}

void SliceController::clear()
{
    if (!has && !sweeping && !dragging) return;
    has = sweeping = dragging = false;
    changed();
}

bool SliceController::band(float& from, float& to) const
{
    if (sweeping) { from = std::min(anchor, cur); to = std::max(anchor, cur); return true; }
    if (!has) return false;
    from = start + dragOffset();
    to   = end   + dragOffset();
    return true;
}

void SliceController::copy() const
{
    if (!has || !pattern) return;
    clipboard().setSlice(kind(), pattern->captureRange(patternId, start, end));
}

void SliceController::deleteRange()
{
    if (!has || !pattern) return;
    has = false;
    pattern->clearRange(patternId, start, end);
    changed();
}

bool SliceController::canPaste() const
{
    return pattern && clipboard().holdsSlice(kind());
}

bool SliceController::pasteAt(float beat)
{
    if (!canPaste()) return false;
    float at = snap > 0.0f ? std::floor(beat / snap + 1e-4f) * snap : beat;
    at = std::max(0.0f, at);
    // Copied first: the paste notifies, and nothing may pull the clipboard out
    // from under it while it runs.
    const SliceClip clip = clipboard().slice;
    if (!pattern->pasteRange(patternId, clip, at)) return false;
    // The pasted range takes the slice over, so it can be dragged into place
    // straight away — as a pasted item selection can.
    has   = true;
    start = at;
    end   = at + clip.length;
    changed();
    return true;
}

void SliceController::draw(int beat0X, int y, int h, int colWidth) const
{
    float from, to;
    if (!band(from, to)) return;
    const int x0 = beat0X + (int)std::lround(from * colWidth);
    const int x1 = beat0X + (int)std::lround(to   * colWidth);
    // Full height and open at top and bottom: the slice runs on through the
    // widgets above and below, so only its two edges are drawn as lines.
    drawBandWash(x0, y, x1 - x0, h);
    fl_color(bandColor);
    fl_line_style(FL_SOLID, 2);
    fl_line(x0, y, x0, y + h);
    fl_line(x1, y, x1, y + h);
    fl_line_style(0);
}
