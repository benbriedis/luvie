#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include "grid.hpp"
#include "popup.hpp"


int main(int argc, char **argv) {
	Fl_Window window(700, 600);
	window.color(0xF0F1F200);

	/* Auto-adding child widgets fouls up. Silly feature anyway. */
	window.end();

	Popup popup{};
	popup.begin();
	popup.end();
	popup.hide();
	window.add(popup);

	std::vector<Note> notes(0);
	MyGrid grid(notes,10,15,30,40,0.25,popup);
	window.add(grid);

	window.show(argc, argv);
	return Fl::run();
}

