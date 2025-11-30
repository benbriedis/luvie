#include "grid.hpp"
#include <FL/fl_draw.H>
#include <cstdio>
#include <iostream>
#include <algorithm>

using std::vector;


MyGrid::MyGrid(vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth): 
	notes(notes),numRows(numRows),numCols(numCols),rowHeight(rowHeight),colWidth(colWidth),
	Fl_Box(0,0,numCols * colWidth,numRows * rowHeight,nullptr) 
{ }

void MyGrid::draw() 
{
//XXX drawing is additive. Dont really need for add 	
	// Call the base class draw method to handle border, label, etc.
	Fl_Box::draw();  //XXX really needed?

	fl_color(FL_BACKGROUND_COLOR); //TODO fix up background colour 
	fl_rectf(x(), y(), w(), h()); 




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

	/* Notes: */
//XXX at what point does copying these small structures become a bad idea?
//	for (const Note& note : notes) { 
	for (const Note note : notes) { 
		int x0 = note.col * colWidth;
		int y0 = note.row * rowHeight;
printf("Drawing note @ %d, %d\n",note.row,note.col);

		/* 
		   Fit slightly inside the grid lines, except at the note start where I'm 
		   lining it up with the start of the beat.
		*/
		fl_rectf(x0,y0-1,colWidth+1,rowHeight+1,0x1111EE00);
	}
}

int MyGrid::handle(int event) 
{
//damage() call may be  useful. Also cf double buffering (and scrolling)

	switch (event) {
		case FL_PUSH: {
				std::cout << "GOT push:" << event << std::endl;	
				toggleNote();
			}
			return 1;
		case FL_DRAG: 
			std::cout << "GOT drag" << event << std::endl;	
//			int t = Fl::event_inside(this);

			//TODO allow a click and drag option to create many options, but probably require a mode to be set
			return 1;
		case FL_RELEASE:
			std::cout << "GOT release" << event << std::endl;	
			//        redraw();
			//        do_callback();
			// never do anything after a callback, as the callback
			// may delete the widget!
			return 1;
		default:
			return Fl_Widget::handle(event);
	}
}

void MyGrid::toggleNote()
{
//TODO adjust for the position of the grid in the windpw
	int ex = Fl::event_x();
	int ey = Fl::event_y();

	int row = ey / rowHeight;
	int col = ex / colWidth;

	/* Remove the note if present */
	int size = notes.size();

	notes.erase(std::remove_if(notes.begin(), notes.end(), 
		[=](const Note& n) { return n.row == row && n.col == col; }), 
		notes.end());

	/* Add the note */
	if (notes.size() == size)
		notes.push_back({row,col});

	redraw();
}


