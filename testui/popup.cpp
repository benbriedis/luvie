#include "popup.hpp"
#include "FL/Fl_Menu_Button.H"


Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"delete",0,0,0,FL_MENU_VALUE},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};


Popup::Popup(int width,int height) : 
	Fl_Menu_Button(-1,-1,0,0)
{
	menu(menutable);
	type(Fl_Menu_Button::POPUP3); 
}

