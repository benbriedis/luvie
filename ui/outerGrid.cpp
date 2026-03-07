#include "outerGrid.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <cstdlib>

OuterGrid::OuterGrid(int x, int y, std::vector<Note> notes, int numRows, int numCols,
                     int rowHeight, int colWidth, float snap, Popup& popup)
	: Fl_Group(x, y, numCols * colWidth, rulerH + numRows * rowHeight),
	  playhead(numCols, colWidth),
	  grid(notes, numRows, numCols, rowHeight, colWidth, snap, popup)
{
	grid.position(x, y + rulerH);
	grid.setPlayhead(&playhead);
	add(grid);
	playhead.setOwner(this);
	end();
}

void OuterGrid::setTransport(ITransport* t, double b, int bpb) {
	playhead.setTransport(t, b, bpb);
	playhead.onEndReached = [this]() { if (onEndReached) onEndReached(); };
}

void OuterGrid::draw() {
	fl_color(rulerBg);
	fl_rectf(x(), y(), w(), rulerH);

	fl_color(rulerBorder);
	fl_line(x(), y() + rulerH - 1, x() + w() - 1, y() + rulerH - 1);

	playhead.drawTriangle(x(), y(), rulerH);

	draw_children();
}

int OuterGrid::handle(int event) {
	bool inRuler = Fl::event_y() >= y() && Fl::event_y() < y() + rulerH;

	switch (event) {
	case FL_PUSH:
		if (inRuler) {
			rulerDragging = true;
			playhead.seek(Fl::event_x(), x());
			if (onSeek) onSeek();
			redraw();
			return 1;
		}
		break;
	case FL_DRAG:
		if (rulerDragging) {
			playhead.seek(Fl::event_x(), x());
			if (onSeek) onSeek();
			redraw();
			return 1;
		}
		break;
	case FL_RELEASE:
		if (rulerDragging) {
			rulerDragging = false;
			return 1;
		}
		break;
	case FL_MOVE:
		if (inRuler) {
			const int grabZone = 8;
			int dist = std::abs(Fl::event_x() - (x() + playhead.xOffset()));
			window()->cursor(dist <= grabZone ? FL_CURSOR_WE : FL_CURSOR_CROSS);
			return 1;
		}
		break;
	case FL_ENTER:
		return 1;  // needed to receive FL_MOVE
	case FL_LEAVE:
		window()->cursor(FL_CURSOR_DEFAULT);
		return 0;
	}
	return Fl_Group::handle(event);
}
