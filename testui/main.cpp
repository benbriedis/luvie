#include "FL/Enumerations.H"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
//#include <FL/Fl_Box.H>

#include "grid.hpp"

int main(int argc, char **argv) {
//XXX can we ditch the new?	
	Fl_Window *window = new Fl_Window(700, 600);
	window->color(0xF0F1F200);

	/*
	Fl_Box *box = new Fl_Box(20, 40, 300, 100,"blah");
	box->box(FL_UP_BOX);
	box->labelfont(FL_BOLD + FL_ITALIC);
	box->labelsize(36);
	box->labeltype(FL_SHADOW_LABEL);
	*/

	MyGrid p(10,20,30,30);
	p.box(FL_UP_BOX);
	p.align(FL_ALIGN_TOP);

	window->end();
	window->show(argc, argv);
	return Fl::run();
}

