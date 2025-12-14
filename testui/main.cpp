#include "FL/Enumerations.H"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include "grid.hpp"
#include "popup.hpp"


int main(int argc, char **argv) {
	Fl_Window window(700, 600);
	window.color(0xF0F1F200);

	std::vector<Note> notes(0);

window.begin(); //XXX internal windows SHOULD be added to this window...

//	Popup popup{};
//	popup.hide();

//	MyGrid grid(notes,10,15,30,40,0.25,&popup);
	MyGrid grid(notes,10,15,30,40,0.25,nullptr);


//	OuterGrid p(notes,10,15,30,40,0.25);
//	p.box(FL_UP_BOX);
//	p.align(FL_ALIGN_TOP);

	window.end();


	Popup popup{};
	popup.hide();

grid.setPopup(&popup);	

	window.add(popup);



	window.show(argc, argv);
	return Fl::run();
}

