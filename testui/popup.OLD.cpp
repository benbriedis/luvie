#include "popup.hpp"
#include "FL/Fl_Menu_Button.H"
#include <iostream>


// Callback function for menu items
void menuCallback(Fl_Widget* w, void* user_data) {
    const Fl_Menu_Item* item = ((Fl_Menu_Button*)w)->mvalue(); // Get the selected item
    std::cout << "Selected: " << item->label() << std::endl;
    std::cout << "Selected item: " << item << std::endl;

return;

    Fl_Menu_Button* menu_button = static_cast<Fl_Menu_Button*>(w);
    const Fl_Menu_Item* chosen_item = menu_button->mvalue(); // Get the chosen item
    if (chosen_item) {
//        fl_alert("Selected: %s", chosen_item->label());
    }
}

Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"delete",0,menuCallback},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};


Popup::Popup(int width,int height) : 
	Fl_Menu_Button(-1,-1,0,0)
{
	menu(menutable);
	type(Fl_Menu_Button::POPUP3); 
}

