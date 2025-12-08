#include "FL/Enumerations.H"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
//#include <FL/Fl_Box.H>

#include "grid.hpp"

int main(int argc, char **argv) {
	Fl_Window window(700, 600);
	window.color(0xF0F1F200);

	/*
	Fl_Box *box = new Fl_Box(20, 40, 300, 100,"blah");
	box->box(FL_UP_BOX);
	box->labelfont(FL_BOLD + FL_ITALIC);
	box->labelsize(36);
	box->labeltype(FL_SHADOW_LABEL);
	*/

	std::vector<Note> notes(0);

	MyGrid p(notes,10,15,30,40,0.25);
	p.box(FL_UP_BOX);
	p.align(FL_ALIGN_TOP);

	window.end();
	window.show(argc, argv);
	return Fl::run();
}

