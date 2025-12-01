#include "grid.hpp"
#include "FL/Enumerations.H"
#include <cstdio>
#include <iostream>
#include <algorithm>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>

using std::vector;


MyGrid::MyGrid(vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth): 
	notes(notes),numRows(numRows),numCols(numCols),rowHeight(rowHeight),colWidth(colWidth),
	selectionState(NONE),
	Fl_Box(0,0,numCols * colWidth,numRows * rowHeight,nullptr) 
{ }

void MyGrid::draw() 
{
//XXX drawing is additive. Dont really need for add 	
	// Call the base class draw method to handle border, label, etc.
	Fl_Box::draw();  //XXX really needed?

	fl_color(FL_BACKGROUND_COLOR); //TODO fix up background colour 
	fl_rectf(x(), y(), w(), h()); 
//XXX Q: are/can backgrounds be transparent?



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
//printf("Drawing note @ %d, %d\n",note.row,note.col);

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
		case FL_PUSH: 
			toggleNote(); //XXX or better on release? What do others do?
			return 1;			// non-zero = we want the event
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

		/* We want mouse events to change the cursor */
		case FL_ENTER: 
			return 1;		// non-zero = we want mouse events

		/*
		   ISSUE not sure at this stage whether notes will (always) be wide enough to easily support resizing AND moving 
		   by hovering the cursor over different parts of the note. MAY want keycombs and/or mode as well/instead.
		*/
		case FL_MOVE: {
			findNoteForCursor();
			return 1;  //XXX or true?
		}
		default:
			return Fl_Widget::handle(event);
	}
}

void MyGrid::findNoteForCursor()
{
	const int resizeZone = 5;

	int x = Fl::event_x();
	int y = Fl::event_y();

	int row = y / rowHeight;
	int col = x / colWidth;

	selectionState = NONE;

	for (Note& n : notes) {
		if (n.row != row)
			continue;

		selectedNote = &n;

		/* Move takes precedence over resize */
		if (n.col == col) {
			window()->cursor(FL_CURSOR_HAND); 
			selectionState = MOVING;
redraw(); //XXX is it needed? Could something less full on be used? Should it be draw()?
			return;
		}


		int leftEdge = n.col * colWidth; 
		int rightEdge = (n.col+1) * colWidth - 1;     //TODO instead of -1 here maybe reflect in length of note??

		if (leftEdge - x <= resizeZone && x - leftEdge <= resizeZone) {
			selectionState = RESIZING;
			side = LEFT;
		}

		if (rightEdge - x <= resizeZone && x - rightEdge <= resizeZone) {
			selectionState = RESIZING;
			side = RIGHT;
		}
	}

//XXX two notes in a row are being connected - perhaps formalise in Notes...
//XXX should not have 'resizing' appear between them.
//XXX also should not be able to resize one note over the other - except perhaps to join
	if (selectionState == RESIZING) {
		window()->cursor(FL_CURSOR_WE); 
}

else 
window()->cursor(FL_CURSOR_DEFAULT); 

redraw();

}

void MyGrid::toggleNote()
{
//TODO adjust for the position of the grid in the window. MAYBE use a "subwindow" so position starts at (0,0)
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


