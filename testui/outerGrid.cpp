#include "outerGrid.hpp"
#include <FL/Fl_Menu_Button.H>
#include <vector>
#include <cstdio>
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
//	{"delete",0,0,0,FL_MENU_VALUE},
	{"delete",0,menuCallback},
	{"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
	{0}
};


OuterGrid::OuterGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap) :
	Fl_Group(0,0,numCols * colWidth,numRows * rowHeight),
	grid(notes,numRows,numCols,rowHeight,colWidth,snap),
	/* Put the capture area offscreen - otherwise it interferes with the grid events */
	popup(-1,-1,0,0)
{
	popup.menu(menutable);
	popup.type(Fl_Menu_Button::POPUP3); 
}

