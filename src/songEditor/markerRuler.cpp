// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "markerRuler.hpp"
#include "noteContextPopup.hpp"
#include "cursors.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cmath>
#include <cstdio>

// Shared by draw() and hitTest() so what is drawn and what is grabbable agree.
static constexpr int flagW    = 44;   // width of a point marker's flag
static constexpr int grabZone = 5;    // pixels either side of a line that grab it

MarkerRuler::MarkerRuler(int x, int y, int w, int h, int numCols, int colWidth,
                         Kind kind, ObservableSong* timeline,
                         MarkerPopup* tempoPopup, MarkerPopup* timeSigPopup)
	: Fl_Widget(x, y, w, h),
	  kind(kind), numCols(numCols), colWidth(colWidth), timeline(timeline),
	  tempoPopup(tempoPopup), timeSigPopup(timeSigPopup)
{
	timeline->addObserver(this);
}

MarkerRuler::~MarkerRuler()
{
	timeline->removeObserver(this);
}

void MarkerRuler::draw()
{
	fl_color(kind == TEMPO ? tempoBg : timeSigBg);
	fl_rectf(x(), y(), w(), h());

	fl_color(0xD1D5DB00);
	fl_line(x(), y() + h() - 1, x() + w() - 1, y() + h() - 1);

	fl_push_clip(x() + clipLeft, y(), w() - clipLeft, h() - 1);
	const int flagH = h() - 2;
	Fl_Color mc = (kind == TEMPO) ? tempoColor : timeSigColor;

	auto drawMarker = [&](int bar, const char* label) {
		int mx = barToPixel(bar);
		fl_color(mc);
		fl_line(mx, y(), mx, y() + h() - 2);
		fl_rectf(mx + 1, y() + 1, flagW, flagH);
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 10);
		fl_draw(label, mx + 1, y() + 1, flagW, flagH, FL_ALIGN_CENTER);
	};

	// A ramp is a band spanning the bars it covers, ending flush with the right
	// edge of its last bar, with a line at each draggable edge.
	auto drawRamp = [&](const BpmMarker& m, const char* label) {
		int lx = barToPixel(m.bar);
		int rx = barToPixel(m.rampEndBar());
		fl_color(fl_color_average(mc, tempoBg, 0.75f));
		fl_rectf(lx + 1, y() + 1, std::max(1, rx - lx - 1), flagH);
		fl_color(mc);
		fl_line(lx, y(), lx, y() + h() - 2);
		fl_line(rx, y(), rx, y() + h() - 2);
		fl_rect(lx, y() + 1, std::max(2, rx - lx + 1), flagH);
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 10);
		fl_draw(label, lx + 1, y() + 1, std::max(1, rx - lx - 1), flagH, FL_ALIGN_CENTER);
	};

	char label[24];
	if (kind == TEMPO) {
		for (auto& m : timeline->get().bpms) {
			if (m.isRamp()) {
				std::snprintf(label, sizeof(label), "%.0f→%.0f", m.bpm, m.endBpm);
				drawRamp(m, label);
			} else {
				std::snprintf(label, sizeof(label), "%.0f", m.bpm);
				drawMarker(m.bar, label);
			}
		}
	} else {
		for (auto& m : timeline->get().timeSigs) {
			// A dotted-note beat (e.g. 6/8 counted in dotted crotchets) is flagged
			// with a trailing '*' — the bar is not simply m.top beats of 1/m.bottom.
			bool dotted = (m.beat == timeSettings::BeatUnit::DottedQuaver ||
			               m.beat == timeSettings::BeatUnit::DottedCrotchet);
			std::snprintf(label, sizeof(label), "%d/%d%s", m.top, m.bottom,
			              dotted ? "*" : "");
			drawMarker(m.bar, label);
		}
	}
	fl_pop_clip();
}

int MarkerRuler::pixelToBar(int px) const
{
	return std::clamp((px - offsetX) / colWidth, 0, numCols - 1);
}

MarkerRuler::Hit MarkerRuler::hitTest(int px) const
{
	auto onFlag = [&](int bar) {
		int mx = barToPixel(bar);
		return std::abs(px - mx) <= grabZone || (px > mx && px < mx + flagW);
	};

	if (kind == TIME_SIG) {
		for (auto& m : timeline->get().timeSigs)
			if (onFlag(m.bar)) return {m.bar, MARKER};
		return {};
	}

	// Ramps first: a band is bigger than a flag, and where a following point
	// marker's flag overlaps it the band's edge is the more useful grab.
	for (auto& m : timeline->get().bpms) {
		if (!m.isRamp()) continue;
		int lx = barToPixel(m.bar);
		int rx = barToPixel(m.rampEndBar());
		if (std::abs(px - lx) <= grabZone) return {m.bar, RAMP_START};
		if (std::abs(px - rx) <= grabZone) return {m.bar, RAMP_END};
		if (px > lx && px < rx)            return {m.bar, RAMP_BODY};
	}
	for (auto& m : timeline->get().bpms)
		if (!m.isRamp() && onFlag(m.bar)) return {m.bar, MARKER};
	return {};
}

bool MarkerRuler::occupied(Kind k, int bar) const
{
	if (k == TEMPO) {
		for (auto& m : timeline->get().bpms)
			if (m.bar == bar) return true;
	} else {
		for (auto& m : timeline->get().timeSigs)
			if (m.bar == bar) return true;
	}
	return false;
}

void MarkerRuler::addMarker(Kind k, int bar)
{
	if (bar < 1 || occupied(k, bar)) return;
	if (k == TEMPO) {
		timeline->setBpm(bar, timeline->bpmAt(bar));
	} else {
		int top, bottom;
		timeline->timeSigAt(bar, top, bottom);
		timeline->setTimeSig(bar, top, bottom, timeline->beatAt(bar));
	}
	openPopupFor(k, bar, /*showDelete=*/false);
}

void MarkerRuler::openPopupFor(Kind k, int bar, bool showDelete)
{
	MarkerPopup* popup = (k == TEMPO) ? tempoPopup : timeSigPopup;
	Fl_Window* win = window();
	Size   avail  = { win ? win->w() : 800, win ? win->h() : 600 };
	Point2 anchor = { barToPixel(bar), y() };
	Point2 pos    = calcPopupPos(avail, anchor, h(), popup->w(), popup->h());

	if (k == TEMPO) {
		auto curve   = timeSettings::TempoCurve::Immediate;
		float bpm    = 120.0f;
		float endBpm = 0.0f;
		if (const BpmMarker* m = timeline->bpmMarkerAt(bar)) {
			curve  = m->curve;
			bpm    = m->bpm;
			endBpm = m->endBpm;
		}

		popup->openTempo(pos.x, pos.y, isFixed(bar), showDelete, curve, bpm, endBpm,
			[this, bar](timeSettings::TempoCurve c, double bpm, double endBpm) {
				// A ramp keeps whatever length it already had; a marker becoming
				// one for the first time gets the default single bar.
				const BpmMarker* m = timeline->bpmMarkerAt(bar);
				int len = (m && m->isRamp()) ? m->lengthBars : 1;
				timeline->setBpmMarker(bar, (float)bpm, c, len, (float)endBpm);
			},
			[this, bar]() { timeline->removeBpm(bar); });
	} else {
		int top = 4, bottom = 4;
		timeSettings::BeatUnit beat = timeSettings::beatUnitDefault;
		for (auto& m : timeline->get().timeSigs)
			if (m.bar == bar) { top = m.top; bottom = m.bottom; beat = m.beat; break; }

		popup->openTimeSig(pos.x, pos.y, isFixed(bar), showDelete, top, bottom, beat,
			[this, bar](int top, int bottom, timeSettings::BeatUnit beat) {
				timeline->setTimeSig(bar, top, bottom, beat);
			},
			[this, bar]() { timeline->removeTimeSig(bar); });
	}
}

// A ramp's edges sit on bar boundaries rather than inside a bar, so they snap to
// the nearest one; a marker being moved snaps to the bar it is dropped in.
int MarkerRuler::pixelToBoundary(int px) const
{
	return std::clamp((int)std::lround((px - offsetX) / (double)colWidth), 0, numCols);
}

void MarkerRuler::dragTo(int px)
{
	const BpmMarker* m = (kind == TEMPO) ? timeline->bpmMarkerAt(draggingBar) : nullptr;

	if (draggingGrab == RAMP_START && m) {
		int newBar = pixelToBoundary(px);
		if (newBar == draggingBar) return;
		// The model clamps both edges; follow the marker to where it landed.
		draggingBar = timeline->resizeBpmRamp(draggingBar, newBar, m->rampEndBar());
		return;
	}
	if (draggingGrab == RAMP_END && m) {
		int newEnd = pixelToBoundary(px);
		if (newEnd == m->rampEndBar()) return;
		timeline->resizeBpmRamp(draggingBar, draggingBar, newEnd);
		return;
	}

	// MARKER / RAMP_BODY: move the whole marker, ramp and all.
	int newBar = std::max(1, pixelToBar(px) - dragBarOffset);
	if (newBar == draggingBar) return;
	if (occupied(kind, newBar)) return;
	if (kind == TEMPO) timeline->moveBpmMarker(draggingBar, newBar);
	else               timeline->moveTimeSigMarker(draggingBar, newBar);
	draggingBar = newBar;
	// redraw triggered by onTimelineChanged()
}

int MarkerRuler::handle(int event)
{
	switch (event) {
	case FL_ENTER:
		return 1;
	case FL_LEAVE:
		window()->cursor(FL_CURSOR_DEFAULT);
		return 0;
	case FL_MOVE: {
		if (Fl::event_x() < x() + clipLeft) {
			window()->cursor(FL_CURSOR_DEFAULT);
			return 1;
		}
		Grab g = hitTest(Fl::event_x()).grab;
		if (g == RAMP_START || g == RAMP_END)
			window()->cursor(FL_CURSOR_WE);
		else
			window()->cursor(contextMenuCursorImage(), 0, 0);
		return 1;
	}
	case FL_PUSH: {
		if (Fl::event_x() < x() + clipLeft) return 1;
		Hit hit = hitTest(Fl::event_x());
		if (Fl::event_button() == FL_LEFT_MOUSE) {
			// Bar 0 is fixed: its marker cannot be moved, but its ramp's right
			// edge can still be dragged.
			bool movable   = hit.bar >= 0
			              && (!isFixed(hit.bar) || hit.grab == RAMP_END);
			draggingBar    = movable ? hit.bar : -1;
			draggingGrab   = movable ? hit.grab : NONE;
			// Grab the band where it was clicked rather than snapping its start
			// under the pointer.
			dragBarOffset  = movable
			               ? pixelToBar(Fl::event_x() - x()) - hit.bar : 0;
		} else if (Fl::event_button() == FL_RIGHT_MOUSE) {
			clickedBar = hit.bar;
		}
		return 1;
	}
	case FL_DRAG: {
		if (draggingBar >= 0)
			dragTo(Fl::event_x() - x());
		return 1;
	}
	case FL_RELEASE: {
		if (Fl::event_button() == FL_LEFT_MOUSE) {
			// Left click no longer creates markers; only marker dragging.
			draggingBar  = -1;
			draggingGrab = NONE;
		} else if (Fl::event_button() == FL_RIGHT_MOUSE) {
			if (clickedBar >= 0) {
				// Right click on an existing marker of this ruler: edit it.
				openPopupFor(kind, clickedBar, /*showDelete=*/true);
			} else {
				// Right click on empty space: create a marker of this ruler's
				// kind at this bar and open its settings popup.
				int bar = std::max(1, pixelToBar(Fl::event_x() - x()));
				addMarker(kind, bar);
			}
			clickedBar = -1;
		}
		return 1;
	}
	}
	return Fl_Widget::handle(event);
}
