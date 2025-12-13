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
	menuButton(0,0,numCols * colWidth,numRows * rowHeight),
	grid(notes,numRows,numCols,rowHeight,colWidth,snap)
{

	menuButton.menu(menutable);
	menuButton.type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)

	end();
}

int OuterGrid::handle(int event) 
{
	Fl_Group::handle(event);

//	if (Fl_Group::handle(event)) 
//		return 1;

	return 0;
/*
	switch (event) {
		case FL_PUSH: 
			if (Fl::event_button() == FL_RIGHT_MOUSE) 
//TODO may prefer to try using show() or something... think this is a better guarantee that the mouse button is the same as the one in Fl_Menu_Button
				//XXX leaving the Fl_Menu_Button to handle...
			{
//menuButton->position (Fl::event_x (), Fl::event_y ());
//menuButton->show ();
//menuButton->popup ();
				return 1;
			}
			printf("outerGrid.cpp handle()  GOT PUSH\n");
			return 0;			// non-zero = we want the event
		default:
			return Fl_Widget::handle(event);
	}
*/
}

