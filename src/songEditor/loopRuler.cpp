#include "loopRuler.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cmath>

static constexpr int flagW    = 44;
static constexpr int grabZone = 5;

LoopRuler::LoopRuler(int x, int y, int w, int h, int numCols, int colWidth)
	: Fl_Widget(x, y, w, h),
	  numCols(numCols), colWidth(colWidth),
	  endBar(numCols - 1)   // End starts flush with the last column
{
}

void LoopRuler::setNumCols(int n)
{
	numCols = std::max(1, n);
	// Keep both markers inside the ruler; End stays at the last column if it was
	// already there (grew with the content), otherwise just clamp.
	endBar   = std::clamp(endBar,   0, numCols - 1);
	startBar = std::clamp(startBar, 0, endBar);
	redraw();
}

void LoopRuler::draw()
{
	fl_color(loopBg);
	fl_rectf(x(), y(), w(), h());

	fl_color(0xD1D5DB00);
	fl_line(x(), y() + h() - 1, x() + w() - 1, y() + h() - 1);

	fl_push_clip(x() + clipLeft, y(), w() - clipLeft, h() - 1);
	const int flagH = h() - 2;

	fl_font(FL_HELVETICA, 10);

	// Start marker: vertical line at the column's left edge, flag to the right.
	{
		int lx = barLeftPixel(startBar);
		fl_color(loopColor);
		fl_line(lx, y(), lx, y() + h() - 2);
		fl_rectf(lx + 1, y() + 1, flagW, flagH);
		fl_color(FL_WHITE);
		fl_draw("Start", lx + 1, y() + 1, flagW, flagH, FL_ALIGN_CENTER);
	}

	// End marker: vertical line at the column's right edge, flag to the left so
	// the marker's right edge is flush with the RHS of its column.
	{
		int rx = barRightPixel(endBar);
		fl_color(loopColor);
		fl_line(rx, y(), rx, y() + h() - 2);
		fl_rectf(rx - flagW, y() + 1, flagW, flagH);
		fl_color(FL_WHITE);
		fl_draw("End", rx - flagW, y() + 1, flagW, flagH, FL_ALIGN_CENTER);
	}

	fl_pop_clip();
}

int LoopRuler::pixelToBar(int px) const
{
	if (colWidth <= 0) return 0;
	return std::clamp((px - x() - offsetX) / colWidth, 0, numCols - 1);
}

LoopRuler::Grab LoopRuler::markerAt(int px) const
{
	int lx = barLeftPixel(startBar);              // Start line
	int rx = barRightPixel(endBar);               // End line

	bool onStart = std::abs(px - lx) <= grabZone || (px > lx && px < lx + flagW);
	bool onEnd   = std::abs(px - rx) <= grabZone || (px < rx && px > rx - flagW);

	if (onStart && onEnd)                         // overlapping flags: pick nearer line
		return std::abs(px - lx) <= std::abs(px - rx) ? START : END;
	if (onStart) return START;
	if (onEnd)   return END;
	return NONE;
}

int LoopRuler::handle(int event)
{
	switch (event) {
	case FL_ENTER:
		return 1;
	case FL_LEAVE:
		window()->cursor(FL_CURSOR_DEFAULT);
		return 0;
	case FL_MOVE: {
		if (Fl::event_x() < x() + clipLeft || markerAt(Fl::event_x()) == NONE)
			window()->cursor(FL_CURSOR_DEFAULT);
		else
			window()->cursor(FL_CURSOR_WE);
		return 1;
	}
	case FL_PUSH: {
		if (Fl::event_x() < x() + clipLeft) return 1;
		if (Fl::event_button() == FL_LEFT_MOUSE)
			dragging = markerAt(Fl::event_x());
		return 1;
	}
	case FL_DRAG: {
		if (dragging == START) {
			int nb = std::clamp(pixelToBar(Fl::event_x()), 0, endBar);
			if (nb != startBar) { startBar = nb; redraw(); }
		} else if (dragging == END) {
			int nb = std::clamp(pixelToBar(Fl::event_x()), startBar, numCols - 1);
			if (nb != endBar) { endBar = nb; redraw(); }
		}
		return 1;
	}
	case FL_RELEASE:
		dragging = NONE;
		return 1;
	}
	return Fl_Widget::handle(event);
}
