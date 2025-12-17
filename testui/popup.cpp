
#include "popup.hpp"
#include "FL/Enumerations.H"
#include "FL/Fl_Window.H"
#include "FL/Fl_Flex.H"
#include "FL/Fl_Button.H"
#include "FL/Fl_Slider.H"
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
    Fl_Window(0,0,0,0)
{
	//XXX haven't been able to size the flex up from its contents. Its meant to be possible...
	//    The children heights are being calculated from the flex box, not the other way around.
	Fl_Flex *flex = new Fl_Flex(0,0,150,100);

    flex->begin();
	Fl_Button *deleteItem = new Fl_Button(0, 0, 0, 0, "Delete");  //XXX remove new?

	//XXX cf a "value" slider instead
	Fl_Slider *slider = new Fl_Slider(0, 0, 150, 30, "Vel");
    slider->type(FL_HOR_NICE_SLIDER);
    slider->box(FL_FLAT_BOX);
	slider->bounds(0.0,1.0);
	slider->value(0.5);
	//flex->resizable(NULL);
    flex->end();

	resize(0,0,flex->w(),flex->h());  //XXX couldnt get resizable to work
	//resizable(flex);
	end();
}

