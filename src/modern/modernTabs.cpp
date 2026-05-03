#include "modernTabs.hpp"
#include <FL/fl_draw.H>

ModernTabs::ModernTabs(int x, int y, int w, int h) : Fl_Tabs(x, y, w, h) {}

void ModernTabs::enableModeToggle(Fl_Color songCol, Fl_Color loopCol)
{
	songColor    = songCol;
	loopColor    = loopCol;
	leftReserve  = toggleW + 1;
	redraw();
}

void ModernTabs::setTabAccent(int tabIdx, Fl_Color c)
{
	if (tabIdx >= (int)tabAccents.size())
		tabAccents.resize(tabIdx + 1, defaultAccent);
	tabAccents[tabIdx] = c;
	redraw();
}

int ModernTabs::tabBarH() const {
	return children() > 0 ? child(0)->y() - y() : 30;
}

void ModernTabs::drawModeToggle(int tbH)
{
	Fl_Color col = modeIsLoop ? loopColor : songColor;
	if (toggleHovered)
		col = fl_color_average(col, FL_WHITE, 0.8f);

	fl_color(col);
	fl_rectf(x(), y(), toggleW, tbH);

	fl_font(FL_HELVETICA_BOLD, labelsize());
	fl_color(FL_WHITE);
	fl_draw(modeIsLoop ? "Loop" : "Song", x(), y(), toggleW, tbH, FL_ALIGN_CENTER);

	// Separator
	fl_color(separator);
	fl_yxline(x() + toggleW, y() + 4, y() + tbH - 2);
}

void ModernTabs::draw() {
	int tbH = tabBarH();
	int n   = children();
	Fl_Widget* v = value();

	// Draw the active child group (content area)
	if (v) {
		if (damage() & ~FL_DAMAGE_CHILD)
			draw_child(*v);
		else
			update_child(*v);
	}

	if (!(damage() & ~FL_DAMAGE_CHILD)) return;

	// Tab bar background
	fl_color(FL_WHITE);
	fl_rectf(x(), y(), w(), tbH);

	// Bottom separator
	fl_color(separator);
	fl_xyline(x(), y() + tbH - 1, x() + w() - 1);

	// Mode toggle button (if enabled)
	if (leftReserve > 0)
		drawModeToggle(tbH);

	if (n == 0) return;

	int tabsX = x() + leftReserve;
	int tabsW = w() - leftReserve;
	int tabW  = tabsW / n;

	for (int i = 0; i < n; i++) {
		Fl_Widget* c = child(i);
		int tx = tabsX + i * tabW;
		int tw = (i == n - 1) ? tabsX + tabsW - tx : tabW;
		bool active = (c == v);

		Fl_Color ac = (i < (int)tabAccents.size()) ? tabAccents[i] : defaultAccent;

		// Tab background
		fl_color(active ? FL_WHITE : inactiveTab);
		fl_rectf(tx, y(), tw, tbH - 1);

		// Vertical divider between tabs
		if (i > 0) {
			fl_color(separator);
			fl_yxline(tx, y() + 4, y() + tbH - 2);
		}

		// Label
		fl_color(FL_FOREGROUND_COLOR);
		fl_font(active ? FL_HELVETICA_BOLD : FL_HELVETICA, labelsize());
		fl_draw(c->label(), tx, y(), tw, tbH, FL_ALIGN_CENTER);

		// Accent underline on active tab
		if (active) {
			fl_color(ac);
			fl_line_style(FL_SOLID, accentH);
			fl_xyline(tx + 8, y() + tbH - 2, tx + tw - 9);
			fl_line_style(0);
		}
	}
}

void ModernTabs::resize(int x, int y, int w, int h) {
	Fl_Widget::resize(x, y, w, h);
	int tbH = tabBarH();
	for (int i = 0; i < children(); i++)
		child(i)->resize(x, y + tbH, w, h - tbH);
}

int ModernTabs::handle(int event) {
	int tbH = tabBarH();
	int ex  = Fl::event_x(), ey = Fl::event_y();
	bool inBar = (ey >= y() && ey < y() + tbH && ex >= x() && ex < x() + w());

	// Mode toggle button interaction
	if (leftReserve > 0 && inBar && ex < x() + toggleW) {
		if (event == FL_MOVE || event == FL_ENTER) {
			if (!toggleHovered) { toggleHovered = true; redraw(); }
			return 1;
		}
		if (event == FL_LEAVE) {
			if (toggleHovered) { toggleHovered = false; redraw(); }
			return 1;
		}
		if (event == FL_PUSH) {
			modeIsLoop = !modeIsLoop;
			redraw();
			if (onModeChanged) onModeChanged(modeIsLoop);
			return 1;
		}
		return 1;
	}

	// Clear toggle hover when mouse moves off it
	if (toggleHovered && (event == FL_MOVE || event == FL_LEAVE)) {
		toggleHovered = false;
		redraw();
	}

	if (event == FL_RELEASE && inBar)
		return 1;

	if (event == FL_PUSH && inBar) {
		int n = children();
		if (n > 0) {
			int tabsX = x() + leftReserve;
			int tabsW = w() - leftReserve;
			if (ex >= tabsX) {
				int idx = (ex - tabsX) * n / tabsW;
				if (idx < 0) idx = 0;
				if (idx >= n) idx = n - 1;
				Fl_Widget* clicked = child(idx);
				if (clicked != value()) {
					value(clicked);
					set_changed();
					redraw();
					do_callback();
				}
			}
		}
		return 1;
	}
	return Fl_Tabs::handle(event);
}
