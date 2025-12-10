#include "FL/Enumerations.H"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
//#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_ask.H> // For fl_alert
#include "grid.hpp"



Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"delete",0,0,0,FL_MENU_VALUE},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};



int main(int argc, char **argv) {
	Fl_Window window(700, 600);
//	window.color(0xF0F1F200);




//Fl_Menu_* menus[4];

    // Create a menu button and set its menu
	Fl_Menu_Button mb(0,0,100000,100000);
//	Fl_Menu_Button mb(0,0,400,400);
//    Fl_Menu_Button mb(10, 10, 80, 25, "Menu");
    mb.menu(menutable);
  //  menu_button->type(Fl_Menu_Button::POPUP1); // Make it a pop-up menu (right-click)
    mb.type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)
//	menu_button.callback(menu_callback);




	std::vector<Note> notes(0);

	MyGrid p(notes,10,15,30,40,0.25);
	p.box(FL_UP_BOX);
	p.align(FL_ALIGN_TOP);



	window.end();
	window.show(argc, argv);
	return Fl::run();
}

