#include "modernTabs.hpp"
#include "editor.hpp"
#include <FL/fl_draw.H>

ModernTabs::ModernTabs(int x, int y, int w, int h) : Fl_Tabs(x, y, w, h) {}

int ModernTabs::tabBarH() const {
	return children() > 0 ? child(0)->y() - y() : 30;
}

void ModernTabs::draw() {
	int tbH = tabBarH();
	int n = children();
	Fl_Widget* v = value();  // also hides non-active children on first call

	// Draw the active child group (content area)
	if (v) {
		if (damage() & ~FL_DAMAGE_CHILD)
			draw_child(*v);
		else
			update_child(*v);
	}

	if (!(damage() & ~FL_DAMAGE_CHILD)) return;

	// Tab bar background
	fl_color(bgColor);
	fl_rectf(x(), y(), w(), tbH);

	// Bottom separator
	fl_color(separator);
	fl_xyline(x(), y() + tbH - 1, x() + w() - 1);

	if (n == 0) return;

	int tabW = w() / n;
	for (int i = 0; i < n; i++) {
		Fl_Widget* c = child(i);
		int tx = x() + i * tabW;
		int tw = (i == n - 1) ? w() - i * tabW : tabW;
		bool active = (c == v);

		// Tab background
		fl_color(active ? bgColor : inactiveTab);
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
			fl_color(accent);
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
	if (event == FL_PUSH) {
		int tbH = tabBarH();
		int ex = Fl::event_x(), ey = Fl::event_y();
		if (ey >= y() && ey < y() + tbH && ex >= x() && ex < x() + w()) {
			int n = children();
			if (n > 0) {
				int idx = (ex - x()) * n / w();
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
			return 1;
		}
	}
	return Fl_Tabs::handle(event);
}
