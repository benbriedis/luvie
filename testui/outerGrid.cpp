#include "outerGrid.hpp"
#include <FL/Fl_Menu_Button.H>
#include <vector>


Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"delete",0,0,0,FL_MENU_VALUE},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};


OuterGrid::OuterGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap) :
	Fl_Group(0,0,numCols * colWidth,numRows * rowHeight),
	menuButton(0,0,numCols * colWidth,numRows * rowHeight),
	grid(notes,numRows,numCols,rowHeight,colWidth,snap)
{

	menuButton.menu(menutable);
	menuButton.type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)


	end();
}

/*
	menuButton = new Fl_Menu_Button(0,0,100000,100000);  //XXX change 100000?
	menuButton->menu(menutable);
	menuButton->type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)
*/	
