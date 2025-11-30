#include "grid.hpp"
// #include <FL/Fl.H>
// #include <FL/Fl_Window.H>
// #include <FL/Fl_Box.H>
#include <FL/fl_draw.H>


MyGrid::MyGrid(int numRows,int numCols,int rowHeight,int colWidth): 
	numRows(numRows),numCols(numCols),rowHeight(rowHeight),colWidth(colWidth),
	Fl_Box(0,0,numCols * colWidth,numRows * rowHeight,nullptr) 
{}

void MyGrid::draw() {
	fl_color(0xEE888800);  //orange

	/* Rows: */
	for (int i = 0; i < numRows+1; i++) {
		int x0 = 0;
		int y0 = i * rowHeight;
		int x1 = numCols * colWidth;
		int y1 = y0; 

		fl_line(x0, y0, x1, y1);
	}

	fl_color(0x00EE0000); //green

	/* Columns: */
	for (int i = 0; i < numCols+1; i++) {
		int x0 = i * colWidth;
		int y0 = 0;
		int x1 = x0; 
		int y1 = numRows * rowHeight;

		fl_line(x0, y0, x1, y1);
	}
}

