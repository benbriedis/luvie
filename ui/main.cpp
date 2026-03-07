#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include "outerGrid.hpp"
#include "popup.hpp"
#include "modernTabs.hpp"
#include "transport.hpp"
#include "simpleTransport.hpp"


static constexpr double bpm        = 120.0;
static constexpr int    beatsPerBar = 4;

int main(int argc, char **argv) {
	const int tabBarH = 35;
	const int bottomH = 50;
	const int winW    = 920;
	const int winH    = tabBarH + OuterGrid::rulerH + 10 * 45 + 20 + bottomH;

	Fl_Double_Window window(winW, winH);
	window.color(bgColor);
	window.end();

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

	std::vector<Note> notes1(0);
	OuterGrid og1(0, tabBarH, notes1, 10, 15, 30, 40, 0.25, popup1);
	tab1.add(og1);

	Fl_Group tab2(0, tabBarH, winW, tabsH - tabBarH, "Song Editor");
	tab2.color(bgColor);
	tabs.add(tab2);

	std::vector<Note> notes2(0);
	OuterGrid og2(0, tabBarH, notes2, 10, 15, 45, 60, 0.25, popup2);
	tab2.add(og2);

	SimpleTransport simpleTransport;
	Transport bottomPane(0, tabsH, winW, bottomH, &simpleTransport, bpm, beatsPerBar);
	window.add(bottomPane);

	og2.setTransport(&simpleTransport, bpm, beatsPerBar);
	og2.onEndReached = [&bottomPane]() { bottomPane.notifyEndReached(); };
	og2.onSeek       = [&bottomPane]() { bottomPane.notifySeek(); };

	window.show(argc, argv);
	return Fl::run();
}
