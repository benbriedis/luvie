
#include "popup.hpp"
#include "FL/Fl_Window.H"
#include "FL/Fl_Flex.H"
#include "FL/Fl_Button.H"
#include <iostream>


// Callback function for menu items
void menuCallback(Fl_Widget* w, void* user_data) {
    const Fl_Menu_Item* item = ((Fl_Menu_Button*)w)->mvalue(); // Get the selected item
    std::cout << "Selected: " << item->label() << std::endl;
    std::cout << "Selected item: " << item << std::endl;

return;

/*
    Fl_Menu_Button* menu_button = static_cast<Fl_Menu_Button*>(w);
    const Fl_Menu_Item* chosen_item = menu_button->mvalue(); // Get the chosen item
    if (chosen_item) {
//        fl_alert("Selected: %s", chosen_item->label());
    }
*/	
}

Fl_Menu_Item menutable[] = {
	{"foo",0,0,0,FL_MENU_INACTIVE},
	{"delete",0,menuCallback},
	{"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
	{0}
};


//XXX are we sure window is the one to use?

Popup::Popup() : 
    Fl_Window(200,200,"blah2"),
//	Fl_Flex(Fl_Flex::VERTICAL)
	flex(0,0,200,200,"blah")
{
    flex.type(Fl_Flex::VERTICAL);
    flex.begin();

    new Fl_Button(0, 0, 0, 0, "Button 1");
    new Fl_Button(0, 0, 0, 0, "Button 2");
    new Fl_Button(0, 0, 0, 0, "Button 3");

    // Optional: set a fixed size for a specific child
    // flex->fixed(button_pointer, 50);

    flex.end();
}

