#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include "grid.hpp"
#include "popup.hpp"
#include "outerGrid.hpp"

//XXX notes ==> cells

class ModernTabs : public Fl_Tabs {
	static constexpr Fl_Color inactiveTab = 0xF3F4F600;  // very light gray
	static constexpr Fl_Color accent      = 0x3B82F600;  // blue
	static constexpr Fl_Color separator   = 0xD1D5DB00;  // light gray border
	static constexpr int      accentH     = 3;

	int tabBarH() const {
		return children() > 0 ? child(0)->y() - y() : 30;
	}

public:
	ModernTabs(int x, int y, int w, int h) : Fl_Tabs(x, y, w, h) {}

	void draw() override {
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

	int handle(int event) override {
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
};


int main(int argc, char **argv) {
	const int tabBarH = 35;
	const int winW = 920;
	const int winH = tabBarH + 10 * 45 + 20;  // fits the larger grid with margin

	Fl_Window window(winW, winH);
	window.color(bgColor);

	/* Auto-adding child widgets fouls up. Silly feature anyway. */
	window.end();

	std::vector<Note> notes1(0);
	std::vector<Note> notes2(0);

	Popup popup1{};
	window.add(popup1);

	Popup popup2{};
	window.add(popup2);

	ModernTabs tabs(0, 0, winW, winH);
	window.add(tabs);

	Fl_Group tab1(0, tabBarH, winW, winH - tabBarH, "Piano Roll");
	tab1.color(bgColor);
	tabs.add(tab1);

	MyGrid grid1(notes1, 10, 15, 30, 40, 0.25, popup1);
	grid1.position(0, tabBarH);
	tab1.add(grid1);

	Fl_Group tab2(0, tabBarH, winW, winH - tabBarH, "Song Editor");
	tab2.color(bgColor);
	tabs.add(tab2);

	MyGrid grid2(notes2, 10, 15, 45, 60, 0.25, popup2);
	grid2.position(0, tabBarH);
	tab2.add(grid2);

	window.show(argc, argv);
	return Fl::run();
}
