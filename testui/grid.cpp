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
			return 1;			// non-zero = we want the event
		case FL_DRAG: {

/* XXX 
   Probable overlap strategy:
   When sliding a note in from the side stick to a bit when the sides meet.
   When notes start to overlap perhaps change cursor to 'forbidden' cursor.
   Bounce back to original location if dropping in a forbidden place.

   Show sides of notes/patterns in different colour.
*/
			Note* selected = &notes[selectedNote];

			if (hoverState==MOVING) {				
				float x = Fl::event_x();
				selected->beat = (x - movingGrabXOffset) / (float)colWidth; 

				/* Ensure the note stays within X bounds */
				if (selected->beat < 0.0)
					selected->beat = 0.0;
				if (selected->beat + selected->length > numBeats)
					selected->beat = numBeats - selected->length;

				float y = Fl::event_y();
				selected->row = (y - movingGrabYOffset + rowHeight/2.0) / (float)rowHeight;

				/* Ensure the note stays within Y bounds */
				if (selected->row < 0)
					selected->row = 0;
				if (selected->row >= numRows)
					selected->row = numRows - 1;

				redraw();	//XXX is a full redraw really required - consider all redraws()?
			}				
			/*
			   NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
			   The second one (optional) might allow it to move relative to the bar.
			*/
//TODO add snap			
//TODO set minimum width			
			if (hoverState==RESIZING) {	
printf("In RESIZING\n");
				float x = Fl::event_x();

				if (side==LEFT) {
					float endBeat = selected->beat + selected->length;
					selected->beat = x / (float)colWidth; 

					if (selected->beat < 0.0)
						selected->beat = 0.0;
					selected->length = endBeat - selected->beat;

					redraw();
				}
				else if (side==RIGHT) {
					selected->length = x / (float)colWidth - selected->beat;

					if (selected->beat + selected->length > numBeats)
						selected->length = numBeats - selected->beat;

					redraw();
				}
				//Right side to change duration...
			}				

			return 1;
		}
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
//XXX better syntax for incorporating this into the loop?
	int i = 0;

	for (Note n : notes) {
		if (n.row != row) {
			i++;
			continue;
		}

		selectedNote = i;

		float leftEdge = n.beat * colWidth; 
		float rightEdge = (n.beat + n.length) * colWidth;

		if (leftEdge - x <= resizeZone && x - leftEdge <= resizeZone) {
			hoverState = RESIZING;
			side = LEFT;
		}
		else if (rightEdge - x <= resizeZone && x - rightEdge <= resizeZone) {
			hoverState = RESIZING;
			side = RIGHT;
		}
		else if (x >= leftEdge && x <= rightEdge) {
			//XXX only required on initial press down
			hoverState = MOVING;
			movingGrabXOffset = x - n.beat * colWidth;
			movingGrabYOffset = y - n.row * rowHeight;

			if (overlapping())
				window()->cursor(FL_CURSOR_WAIT); 
			else
				window()->cursor(FL_CURSOR_HAND); 
//				window()->cursor(FL_CURSOR_CROSS); 

			redraw();
			/* Move takes precedence over resizing any neighbouring notes */
			
printf("GOT MOVING  selectedNote: %d",i);			

			return;
		}

		i++;
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

bool MyGrid::overlapping()
{
	float x = Fl::event_x();
	int y = Fl::event_y();

	int row = y / rowHeight;
	float beat = x / colWidth;

//TODO convert other iterators

	int i = 0; //XXX again, can we incorporate this into the loop?

printf("selectedNote: %d\n",selectedNote);	

//	for (auto note = notes.begin(); note != notes.end(); ++note) {
	for (const auto note : notes) {
printf("i: %d\n",i);	
		if (i == selectedNote || note.row != row) {
			i++;
			continue;
		}

		Note a = notes[selectedNote];
		Note b = note;

		if (a.beat > b.beat && a.beat < b.beat + b.length)
			return true;

		if (a.beat + a.length > b.beat  &&  a.beat + a.length < b.beat + b.length)
			return true;

		i++;
	}
	return false;
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


