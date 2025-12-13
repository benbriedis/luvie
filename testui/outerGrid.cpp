#include "outerGrid.hpp"
#include <FL/Fl_Menu_Button.H>
#include <vector>
#include <cstdio>


OuterGrid::OuterGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap) :
	Fl_Group(0,0,numCols * colWidth,numRows * rowHeight),
	grid(notes,numRows,numCols,rowHeight,colWidth,snap),
	popup(numCols * colWidth,numRows * rowHeight)
{
//XXX do we need this end()?	
	end();
}

