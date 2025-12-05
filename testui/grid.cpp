#include "grid.hpp"
#include "FL/Enumerations.H"
#include <cstdio>
#include <iostream>
#include <algorithm>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>

using std::vector;


MyGrid::MyGrid(vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth): 
	notes(notes),numRows(numRows),numBeats(numCols),rowHeight(rowHeight),colWidth(colWidth),
	hoverState(NONE),
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
		int x1 = numBeats * colWidth;
		int y1 = y0; 

		fl_line(x0, y0, x1, y1);
	}

	fl_color(0x00EE0000); //green

	/* Columns: */
	for (int i = 0; i < numBeats+1; i++) {
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
		int x0 = note.beat * colWidth;  //TODO round to nearest?
		int y0 = note.row * rowHeight;

//TODO prevent to note from being so short its invisible (Q: how to handle very short notes?)		
		int width = note.length * colWidth;
//printf("Drawing note @ %d, %d\n",note.row,note.col);

		/* 
		   Fit slightly inside the grid lines, except at the note start where I'm 
		   lining it up with the start of the beat.
		*/
		fl_rectf(x0,y0-1,width,rowHeight+1,0x1111EE00);
	}
}

int MyGrid::handle(int event) 
{
//damage() call may be  useful. Also cf double buffering (and scrolling)

	switch (event) {
		case FL_PUSH: 
			std::cout << "GOT push" << event << std::endl;	
			return 1;			// non-zero = we want the event
		case FL_DRAG: 

//TODO probably merge adjacent/overlapping notes

			if (hoverState==MOVING) {				
				float x = Fl::event_x();
				selectedNote->beat = (x - movingGrabOffset) / (float)colWidth; 

				/* Ensure the note stays within bounds */
				if (selectedNote->beat < 0.0)
					selectedNote->beat = 0.0;
				if (selectedNote->beat + selectedNote->length > numBeats)
					selectedNote->beat = numBeats - selectedNote->length;

				redraw();	//XXX is a full redraw really required - consider all redraws()?
			}				
			/*
			   NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
			   The second one (optional) might allow it to move relative to the bar.
			*/
//TODO add snap			
			if (hoverState==RESIZING) {				
				float x = Fl::event_x();

				if (side==LEFT) {
					float endBeat = selectedNote->beat + selectedNote->length;
					selectedNote->beat = x / (float)colWidth; 

					if (selectedNote->beat < 0.0)
						selectedNote->beat = 0.0;
					selectedNote->length = endBeat - selectedNote->beat;

					redraw();
				}
				else if (side==RIGHT) {
					selectedNote->length = x / (float)colWidth - selectedNote->beat;

					if (selectedNote->beat + selectedNote->length > numBeats)
						selectedNote->length = numBeats - selectedNote->beat;

					redraw();
				}
				//Right side to change duration...
			}				

			return 1;
		case FL_RELEASE:
//TODO a delete note option is now required

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

void MyGrid::findNoteForCursor()
{
	const int resizeZone = 5;

	float x = Fl::event_x();
	int y = Fl::event_y();

	int row = y / rowHeight;
	float beat = x / colWidth;

	hoverState = NONE;

	for (Note& n : notes) {
		if (n.row != row)
			continue;

		selectedNote = &n;

		float leftEdge = n.beat * colWidth; 
		float rightEdge = (n.beat + n.length) * colWidth;

		/* Move takes precedence over resize */
		if (x >= leftEdge && x <= rightEdge) {
			window()->cursor(FL_CURSOR_HAND); 
			hoverState = MOVING;
			movingGrabOffset = x - selectedNote->beat * colWidth;
			redraw();
			return;
		}

		if (leftEdge - x <= resizeZone && x - leftEdge <= resizeZone) {
			hoverState = RESIZING;
			side = LEFT;
		}

		if (rightEdge - x <= resizeZone && x - rightEdge <= resizeZone) {
			hoverState = RESIZING;
			side = RIGHT;
		}
	}

//XXX two notes in a row are being connected - perhaps formalise in Notes...
//XXX should not have 'resizing' appear between them.
//XXX also should not be able to resize one note over the other - except perhaps to join
	if (hoverState == RESIZING) {
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
	notes.erase(std::remove_if(notes.begin(), notes.end(), 
		[=](const Note& n) { return n.row == row && n.beat == col; }), 
		notes.end());

//TODO add option to split the beats and zoom in or out
//     Probably best to keep using (pitch,beat,length) and to create a column() function and then use that to perform
//     calcs rather than col.  (Probably dont want for the SongEditor gui though)

//TODO allow multiple additions using a single click and drag

	/* Add the note */
	if (notes.size() == size) {
		/* Disallow note creation in partly occupied cells (too unclear to allow this behaviour) */
		bool clear = true;
		vector<Note>::iterator it = notes.begin();
		while (it != notes.end() && clear) {
			clear = it->row != row || (col < (int)it->beat || col > (int)(it->beat + it->length - 0.000000001));  //HACK
			it++;
		}
		if (clear)
			notes.push_back({row,col,1.0});
	}

	redraw();
}


