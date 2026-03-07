#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include "grid.hpp"
#include "popup.hpp"
#include "outerGrid.hpp"
#include "modernTabs.hpp"
#include "transport.hpp"
#include "simpleTransport.hpp"
#include "playheadRuler.hpp"


static constexpr double bpm        = 120.0;
static constexpr int    beatsPerBar = 4;

int main(int argc, char **argv) {
	const int tabBarH = 35;
	const int rulerH  = 20;
	const int bottomH = 50;
	const int winW    = 920;
	const int winH    = tabBarH + rulerH + 10 * 45 + 20 + bottomH;

	Fl_Double_Window window(winW, winH);
	window.color(bgColor);

	/* Auto-adding child widgets fouls up. Silly feature anyway. */
	window.end();

	std::vector<Note> notes1(0);
	std::vector<Note> notes2(0);

	Popup popup1{};
	window.add(popup1);

	Popup popup2{};
	window.add(popup2);

	const int tabsH = winH - bottomH;

	ModernTabs tabs(0, 0, winW, tabsH);
	window.add(tabs);

	Fl_Group tab1(0, tabBarH, winW, tabsH - tabBarH, "Piano Roll");
	tab1.color(bgColor);
	tabs.add(tab1);

	PlayheadRuler ruler1(0, tabBarH, 15 * 40, rulerH, 15, 40);
	tab1.add(ruler1);

	MyGrid grid1(notes1, 10, 15, 30, 40, 0.25, popup1);
	grid1.position(0, tabBarH + rulerH);
	tab1.add(grid1);

	Fl_Group tab2(0, tabBarH, winW, tabsH - tabBarH, "Song Editor");
	tab2.color(bgColor);
	tabs.add(tab2);

	PlayheadRuler ruler2(0, tabBarH, 15 * 60, rulerH, 15, 60);
	tab2.add(ruler2);

	MyGrid grid2(notes2, 10, 15, 45, 60, 0.25, popup2);
	grid2.position(0, tabBarH + rulerH);
	tab2.add(grid2);

	SimpleTransport simpleTransport;
	Transport bottomPane(0, tabsH, winW, bottomH, &simpleTransport, bpm, beatsPerBar);
	window.add(bottomPane);

	grid2.setTransport(&simpleTransport, bpm, beatsPerBar);
	grid2.onEndReached = [&bottomPane]() { bottomPane.notifyEndReached(); };

	ruler2.setTransport(&simpleTransport, bpm, beatsPerBar);
	ruler2.setOnSeek([&grid2]() { grid2.redraw(); });

	window.show(argc, argv);
	return Fl::run();
}
