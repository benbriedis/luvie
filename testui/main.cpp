#include "FL/Enumerations.H"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
//#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_ask.H> // For fl_alert

//#include "grid.hpp"


// Callback function for menu items
void menu_callback(Fl_Widget* w, void* user_data) {

	printf("IN callback()\n");

/*
    Fl_Menu_Button* menu_button = static_cast<Fl_Menu_Button*>(w);
    const Fl_Menu_Item* chosen_item = menu_button->mvalue(); // Get the chosen item
    if (chosen_item) {
        fl_alert("Selected: %s", chosen_item->label());
    }
*/	
}


int main(int argc, char **argv) {
	Fl_Window window(700, 600);
	window.color(0xF0F1F200);




    // Define menu items
    Fl_Menu_Item menu_items[] = {
        {"Item 1", FL_COMMAND + '1', menu_callback, 0, 0},
        {"Item 2", FL_COMMAND + '2', menu_callback, 0, 0},
        {"Submenu", 0, 0, 0, FL_SUBMENU},
            {"Sub-item A", 0, menu_callback, 0, 0},
            {"Sub-item B", 0, menu_callback, 0, 0},
            {0}, // End of submenu
        {0} // End of menu
    };

//Fl_Menu_* menus[4];

    // Create a menu button and set its menu
    Fl_Menu_Button* menu_button = new Fl_Menu_Button(10, 10, 80, 25, "Menu");
    menu_button->menu(menu_items);
  //  menu_button->type(Fl_Menu_Button::POPUP1); // Make it a pop-up menu (right-click)
    menu_button->type(Fl_Menu_Button::POPUP1); // Make it a pop-up menu (right-click)
	menu_button->callback(menu_callback);
	window.resizable(menu_button);


/*



	std::vector<Note> notes(0);

	MyGrid p(notes,10,15,30,40,0.25);
	p.box(FL_UP_BOX);
	p.align(FL_ALIGN_TOP);
*/	



	window.end();
	window.show(argc, argv);
	return Fl::run();
}

