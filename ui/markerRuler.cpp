#include "markerRuler.hpp"
#include "popup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cstdio>

MarkerRuler::MarkerRuler(int x, int y, int w, int h, int numCols, int colWidth,
                         Kind kind, ObservableTimeline* timeline, MarkerPopup* popup)
	: Fl_Widget(x, y, w, h),
	  kind(kind), numCols(numCols), colWidth(colWidth), timeline(timeline), popup(popup)
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
	static constexpr int flagW = 44;
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

	char label[16];
	if (kind == TEMPO) {
		for (auto& m : timeline->get().bpms) {
			std::snprintf(label, sizeof(label), "%.0f", m.bpm);
			drawMarker(m.bar, label);
		}
	} else {
		for (auto& m : timeline->get().timeSigs) {
			std::snprintf(label, sizeof(label), "%d/%d", m.top, m.bottom);
			drawMarker(m.bar, label);
		}
	}
	fl_pop_clip();
}

int MarkerRuler::pixelToBar(int px) const
{
	return std::clamp((px - offsetX) / colWidth, 0, numCols - 1);
}

int MarkerRuler::findBarAt(int px) const
{
	static constexpr int grabZone = 5;
	static constexpr int flagW    = 44;

	auto check = [&](int bar) {
		int mx = barToPixel(bar);
		return std::abs(px - mx) <= grabZone || (px > mx && px < mx + flagW);
	};

	if (kind == TEMPO) {
		for (auto& m : timeline->get().bpms)
			if (check(m.bar)) return m.bar;
	} else {
		for (auto& m : timeline->get().timeSigs)
			if (check(m.bar)) return m.bar;
	}
	return -1;
}

void MarkerRuler::openPopupFor(int bar)
{
	Fl_Window* win = window();
	Size   avail  = { win ? win->w() : 800, win ? win->h() : 600 };
	Point2 anchor = { barToPixel(bar), y() };
	Point2 pos    = calcPopupPos(avail, anchor, h(), popup->w(), popup->h());

	if (kind == TEMPO) {
		float bpm = 120.0f;
		for (auto& m : timeline->get().bpms)
			if (m.bar == bar) { bpm = m.bpm; break; }

		popup->openTempo(pos.x, pos.y, isFixed(bar), bpm,
			[this, bar](double bpm) { timeline->setBpm(bar, (float)bpm); },
			[this, bar]()           { timeline->removeBpm(bar); });
	} else {
		int top = 4, bottom = 4;
		for (auto& m : timeline->get().timeSigs)
			if (m.bar == bar) { top = m.top; bottom = m.bottom; break; }

		popup->openTimeSig(pos.x, pos.y, isFixed(bar), top, bottom,
			[this, bar](int top, int bottom) { timeline->setTimeSig(bar, top, bottom); },
			[this, bar]()                    { timeline->removeTimeSig(bar); });
	}
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
		int bar = findBarAt(Fl::event_x());
		Fl_Cursor cur = FL_CURSOR_CROSS;
		if (bar >= 0) cur = isFixed(bar) ? FL_CURSOR_DEFAULT : FL_CURSOR_WE;
		window()->cursor(cur);
		return 1;
	}
	case FL_PUSH: {
		if (Fl::event_button() == FL_LEFT_MOUSE) {
			int bar = findBarAt(Fl::event_x());
			draggingBar = (bar >= 0 && !isFixed(bar)) ? bar : -1;
			didDrag     = false;
		} else if (Fl::event_button() == FL_RIGHT_MOUSE) {
			clickedBar = findBarAt(Fl::event_x());
		}
		return 1;
	}
	case FL_DRAG: {
		if (draggingBar >= 0) {
			int newBar = std::max(1, pixelToBar(Fl::event_x() - x()));
			if (newBar != draggingBar) {
				bool occupied = false;
				if (kind == TEMPO) {
					for (auto& m : timeline->get().bpms)
						if (m.bar == newBar) { occupied = true; break; }
				} else {
					for (auto& m : timeline->get().timeSigs)
						if (m.bar == newBar) { occupied = true; break; }
				}
				if (!occupied) {
					if (kind == TEMPO)
						timeline->moveBpmMarker(draggingBar, newBar);
					else
						timeline->moveTimeSigMarker(draggingBar, newBar);
					draggingBar = newBar;
					didDrag = true;
					// redraw triggered by onTimelineChanged()
				}
			}
		}
		return 1;
	}
	case FL_RELEASE: {
		if (Fl::event_button() == FL_LEFT_MOUSE) {
			if (!didDrag && draggingBar < 0) {
				// Left click on empty space: create marker and open popup
				int bar = std::max(1, pixelToBar(Fl::event_x() - x()));
				bool occupied = false;
				if (kind == TEMPO) {
					for (auto& m : timeline->get().bpms)
						if (m.bar == bar) { occupied = true; break; }
				} else {
					for (auto& m : timeline->get().timeSigs)
						if (m.bar == bar) { occupied = true; break; }
				}
				if (!occupied) {
					if (kind == TEMPO)
						timeline->setBpm(bar, timeline->bpmAt(bar));
					else {
						int top, bottom;
						timeline->timeSigAt(bar, top, bottom);
						timeline->setTimeSig(bar, top, bottom);
					}
					openPopupFor(bar);
				}
			}
			draggingBar = -1;
			didDrag     = false;
		} else if (Fl::event_button() == FL_RIGHT_MOUSE) {
			if (clickedBar >= 0)
				openPopupFor(clickedBar);
			clickedBar = -1;
		}
		return 1;
	}
	}
	return Fl_Widget::handle(event);
}
