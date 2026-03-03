#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include "grid.hpp"
#include "popup.hpp"
#include "outerGrid.hpp"
#include "modernTabs.hpp"


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
