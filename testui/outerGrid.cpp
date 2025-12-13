#include "outerGrid.hpp"
#include <FL/Fl_Menu_Button.H>
#include <vector>
#include <cstdio>


Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"delete",0,0,0,FL_MENU_VALUE},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};


OuterGrid::OuterGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap) :
	Fl_Group(0,0,numCols * colWidth,numRows * rowHeight),
	grid(notes,numRows,numCols,rowHeight,colWidth,snap),
	popup(-1,-1,0,0)
{
	popup.menu(menutable);
	popup.type(Fl_Menu_Button::POPUP3); 

//XXX do we need this end()?	
	end();
}

