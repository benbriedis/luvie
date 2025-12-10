#include "grid.hpp"
#include "FL/Enumerations.H"
#include <cstdio>
#include <ranges>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H> // For fl_alert
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_ask.H> // For fl_alert

//import std;

using std::vector;

// Callback function for menu items
/*
void menu_callback(Fl_Widget* w, void* user_data) {
    Fl_Menu_Button* menu_button = static_cast<Fl_Menu_Button*>(w);
    const Fl_Menu_Item* chosen_item = menu_button->mvalue(); // Get the chosen item
    if (chosen_item) {
        fl_alert("Selected: %s", chosen_item->label());
    }
}
*/

Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"delete",0,0,0,FL_MENU_VALUE},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};
//Fl_Menu_Button mb(0,0,100000,100000);


MyGrid::MyGrid(vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap): 
	notes(notes),numRows(numRows),numCols(numCols),rowHeight(rowHeight),colWidth(colWidth),snap(snap),
	hoverState(NONE),
	Fl_Box(0,0,numCols * colWidth,numRows * rowHeight,nullptr) 
{ 
//	Fl_Menu_Button mb(0,0,100000,100000);
//	mb.menu(menutable);
//	mb.type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)
//	menu_button.callback(menu_callback);

/*	
	Fl_Menu_Button* mb = new Fl_Menu_Button(0,0,100000,100000);
	mb->menu(menutable);
	mb->type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)
*/	

	menuButton = new Fl_Menu_Button(0,0,100000,100000);  //XXX change 100000?
	menuButton->menu(menutable);
	menuButton->type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)
}

void MyGrid::init() 
{
//	Fl_Menu_Button mb(0,0,100000,100000);
//	mb.menu(menutable);
//	mb.type(Fl_Menu_Button::POPUP3); // Make it a pop-up menu (right-click)
//	menu_button.callback(menu_callback);
}	

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
	for (const Note note : notes) { 
		int x0 = note.col * colWidth;  //TODO round to nearest?
		int y0 = note.row * rowHeight;

//TODO prevent to note from being so short its invisible (Q: how to handle very short notes?)		
		int width = note.length * colWidth;
//printf("Drawing note @ %d, %d\n",note.row,note.col);

		/* 
		   Fit slightly inside the grid lines, except at the note start where I'm 
		   lining it up with the start of the column.
		*/
		fl_rectf(x0,y0+1,width,rowHeight-1,0x5555EE00);
		fl_color(0x1111EE00);
		fl_line_style(FL_SOLID,5);
		fl_line(x0,y0+1,x0,y0+rowHeight-1);
		fl_line_style(0);
	}
}

int MyGrid::handle(int event) 
{
//damage() call may be  useful. Also cf double buffering (and scrolling)

	switch (event) {
  //  menu_button->type(Fl_Menu_Button::POPUP1); // Make it a pop-up menu (right-click)
		case FL_PUSH: 
			if (Fl::event_button() == FL_RIGHT_MOUSE) 
//TODO may prefer to try using show() or something... think this is a better guarantee that the mouse button is the same as the one in Fl_Menu_Button
				//XXX leaving the Fl_Menu_Button to handle...
			{
menuButton->position (Fl::event_x (), Fl::event_y ());
menuButton->show ();
menuButton->popup ();
				return 1;
			}

			return 1;			// non-zero = we want the event
		case FL_DRAG: 
			if (hoverState==MOVING) 
				moving();

			if (hoverState==RESIZING) 
				resizing();

			return 1;

		case FL_RELEASE:
			if (hoverState==MOVING && amOverlapping) {
				// TODO maybe drift back or fade out and in note
				notes[selectedNote].row = pickupPoint.row;
				notes[selectedNote].col = pickupPoint.col;
				redraw();
			}

			if (hoverState==NONE)
				toggleNote();
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

void MyGrid::moving()
{
	Note* selected = &notes[selectedNote];

	float x = Fl::event_x();
	selected->col = (x - movingGrabXOffset) / (float)colWidth; 

	/* Ensure the note stays within X bounds */
	if (selected->col < 0.0)
		selected->col = 0.0;
	if (selected->col + selected->length > numCols)
		selected->col = numCols - selected->length;

	/* Apply snap */
	if (snap > 0.0) 
		selected->col = std::round(selected->col / snap) * snap;

	float y = Fl::event_y();
	selected->row = (y - movingGrabYOffset + rowHeight/2.0) / (float)rowHeight;

	/* Ensure the note stays within Y bounds */
	if (selected->row < 0)
		selected->row = 0;
	if (selected->row >= numRows)
		selected->row = numRows - 1;

//TODO find or implement a no-drop / not-allow / forbidden icon (circle with cross through it, or just X)
	amOverlapping = overlappingNote() >= 0;
	window()->cursor(amOverlapping ? FL_CURSOR_WAIT : FL_CURSOR_HAND); 

	redraw();	//XXX is a full redraw really required - consider all redraws()?
}

/*
   NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
   The second one (optional) might allow it to move relative to the bar.
*/
void MyGrid::resizing()
{
//TODO add snap / magnetism
//XXX if changing grid size want the num of pixels to remain constant.
	float minLength = 10.0 / colWidth;

	Note* selected = &notes[selectedNote];

	float x = Fl::event_x();

//TODO restrict length to a certain minimum				
	if (side==LEFT) {
		float endCol = selected->col + selected->length;
		selected->col = x / (float)colWidth; 

		/* Apply snap: */
		if (snap)
			selected->col = std::round(selected->col / snap) * snap;

		int neighbour = overlappingNote();
		float min = neighbour < 0 ? 0.0 : notes[neighbour].col + notes[neighbour].length;
		if (selected->col < min)
			selected->col = min;

		selected->length = endCol - selected->col;

		if (selected->length < minLength) {
			selected->length = minLength;
			selected->col = endCol - minLength;
		}

		redraw();
	}
	else if (side==RIGHT) {
		selected->length = x / (float)colWidth - selected->col;

		/* Apply snap: */
		float endCol = selected->col + selected->length;
		if (snap) {
			endCol = std::round(endCol / snap) * snap;
			selected->length = endCol - selected->col;
		}

		int neighbour = overlappingNote();
		float max = neighbour < 0 ? numCols : notes[neighbour].col;
		if (selected->col + selected->length > max)
			selected->length = max - selected->col;

		if (selected->length < minLength)
			selected->length = minLength;

		redraw();
	}
	//Right side to change duration...
}

void MyGrid::findNoteForCursor()
{
	const int resizeZone = 5;

	float x = Fl::event_x();
	int y = Fl::event_y();

	int row = y / rowHeight;
	float col = x / colWidth;

	int selectedIfResize = 0;

	hoverState = NONE;

	for (const auto [i,n]: std::views::enumerate(notes)) {
		if (n.row != row) 
			continue;

		float leftEdge = n.col * colWidth; 
		float rightEdge = (n.col + n.length) * colWidth;

		if (leftEdge - x <= resizeZone && x - leftEdge <= resizeZone) {
			hoverState = RESIZING;
			side = LEFT;
			selectedIfResize = i;
		}
		else if (rightEdge - x <= resizeZone && x - rightEdge <= resizeZone) {
			hoverState = RESIZING;
			side = RIGHT;
			selectedIfResize = i;
		}
		else if (x >= leftEdge && x <= rightEdge) {
			//XXX only required on initial press down
			hoverState = MOVING;
			selectedNote = i;
			movingGrabXOffset = x - n.col * colWidth;
			movingGrabYOffset = y - n.row * rowHeight;
			pickupPoint = {n.row,n.col};

			window()->cursor(FL_CURSOR_HAND); 
//			window()->cursor(FL_CURSOR_CROSS); 

			redraw();
			/* Move takes precedence over resizing any neighbouring notes */
			
			return;
		}
	}

	if (hoverState == RESIZING) {
		selectedNote = selectedIfResize;
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
	float col = (float)(ex / colWidth);

	/* Remove the note if present */
	int size = notes.size();

//FIXME delete - need keycombo, right mouse key, or mode to distinguish from move. 
//      Delete should probably be the exception I think rather than the rule as its probably a bit less common and 
//      move requires more control. Cf using a right mouse shortcut menu? Maybe put velocity in there too?
//TODO can we use a range instead?	
	notes.erase(std::remove_if(notes.begin(), notes.end(), 
		[=](const Note& n) { return n.row == row && n.col == col; }), 
		notes.end());

//TODO add option to split the columns and zoom in or out
//     Probably best to keep using (pitch,column,length) and to create a column() function and then use that to perform
//     calcs rather than col.  (Probably dont want for the SongEditor gui though)

//TODO allow multiple additions using a single click and drag (sequencer only)

	/* Add the note */
	if (notes.size() == size) {
		/* Disallow note creation in partly occupied cells (too unclear to allow this behaviour) */
		bool clear = true;
		for (const Note n : notes) { 
			clear = n.row != row || (col < (int)n.col || col > (int)(n.col + n.length - 0.000000001));  //HACK
			if (clear)
				break;
		}
		if (clear)
			notes.push_back({row,col,1.0});
	}

	redraw();
}

/* Returns the note index, or -1 */
int MyGrid::overlappingNote()
{
	Note a = notes[selectedNote];
	float aStart = a.col;
	float aEnd = a.col + a.length;

	for (const auto [i,b]: std::views::enumerate(notes)) {
		if (i == selectedNote || b.row != a.row) 
			continue;

		float bStart = b.col;
		float bEnd = b.col + b.length;

		float firstEnd = aStart <= bStart ? aEnd : bEnd;
		float secondStart = aStart <= bStart ? bStart : aStart;

		if (firstEnd > secondStart)
			return i;
	}
	return -1;
}

