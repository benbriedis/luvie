#include "grid.hpp"
#include "outerGrid.hpp"
#include <FL/Fl.H>
#include "cell.hpp"
#include "FL/Enumerations.H"
#include "popup.hpp"
#include <cstdio>
#include <iostream>
#include <ranges>
#include <algorithm>
#include <cmath>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_RGB_Image.H>
#include "cursors.hpp"
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

//Fl_Menu_Button mb(0,0,100000,100000);

//FIXME clicking before or after cell creates an overlapping cell 

//FIXME leading front line narrows when putting next to another cell


MyGrid::MyGrid(vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap,Popup& popup) : 
	notes(notes),numRows(numRows),numCols(numCols),rowHeight(rowHeight),colWidth(colWidth),snap(snap),popup(popup),
	hoverState(NONE),creationForbidden(false),
	Fl_Box(0,0,numCols * colWidth,numRows * rowHeight,nullptr) 
{ 
}

void MyGrid::draw() 
{
//XXX drawing is additive. Dont really need for add 	
	// Call the base class draw method to handle border, label, etc.
	Fl_Box::draw();  //XXX really needed?

	fl_color(bgColor);
	fl_rectf(x(), y(), w(), h()); 
//XXX Q: are/can backgrounds be transparent?



	fl_color(0xEE888800);  //orange

	/* Rows: */
	for (int i = 0; i < numRows+1; i++) {
		int x0 = x();
		int y0 = y() + i * rowHeight;
		int x1 = x() + numCols * colWidth;
		int y1 = y0;

		fl_line(x0, y0, x1, y1);
	}

	fl_color(0x00EE0000); //green

	/* Columns: */
	for (int i = 0; i < numCols+1; i++) {
		int x0 = x() + i * colWidth;
		int y0 = y();
		int x1 = x0;
		int y1 = y() + numRows * rowHeight;

		fl_line(x0, y0, x1, y1);
	}

	/* Notes: */
//XXX at what point does copying these small structures become a bad idea?
	for (const Note note : notes) {
		int x0 = x() + note.col * colWidth;  //TODO round to nearest?
		int y0 = y() + note.row * rowHeight;

//TODO prevent to note from being so short its invisible (Q: how to handle very short notes?)
		int width = note.length * colWidth;

		/*
		   Fit slightly inside the grid lines, except at the note start where I'm
		   lining it up with the start of the column.
		*/
		fl_rectf(x0,y0+1,width,rowHeight-1,0x5555EE00);
		const int barWidth = 5;
		fl_color(0x1111EE00);
		fl_line_style(FL_SOLID, barWidth);
		fl_line(x0 + barWidth/2, y0+1, x0 + barWidth/2, y0+rowHeight-1);
		fl_line_style(0);
	}

	/* Position line: drawn last so it appears over notes */
	if (transport) {
		double posInBars = transport->position() * bpm / 60.0 / beatsPerBar;
		int lineX = x() + (int)(posInBars * colWidth);
		if (lineX > x() + numCols * colWidth - 2)
			lineX = x() + numCols * colWidth - 2;
		fl_color(0xEF444400);  // red
		fl_line_style(FL_SOLID, 2);
		fl_line(lineX, y(), lineX, y() + numRows * rowHeight);
		fl_line_style(0);
	}
}

int MyGrid::handle(int event) 
{

//damage() call may be  useful. Also cf double buffering (and scrolling)

	if (popup.visible())
		return 0;

	switch (event) {
		case FL_PUSH:
			if (Fl::event_button() == FL_RIGHT_MOUSE) {
				//XXX is a callback desirable here?
				popup.open(selectedNote,&notes,this);
				popup.show();
			} else if (hoverState == NONE) {
				int row = (Fl::event_y() - y()) / rowHeight;
				float col = (float)((Fl::event_x() - x()) / colWidth);
				bool wouldRemove = std::any_of(notes.begin(), notes.end(),
					[=](const Note& n) { return n.row == row && n.col == col; });
				if (!wouldRemove) {
					creationForbidden = std::any_of(notes.begin(), notes.end(),
						[=](const Note& n) { return n.row == row && col < n.col + n.length && col + 1.0f > n.col; });
					if (creationForbidden)
						window()->cursor(forbiddenCursorImage(), 11, 11);
				}
			}
			return 1;

		case FL_DRAG: 
			if (hoverState==MOVING) 
				moving();

			if (hoverState==RESIZING) 
				resizing();

			return 1;

		case FL_RELEASE:
			if (hoverState==MOVING && amOverlapping) {
				notes[selectedNote].row = lastValidPosition.row;
				notes[selectedNote].col = lastValidPosition.col;
				redraw();
			}

			if (hoverState==NONE) {
				if (!creationForbidden)
					toggleNote();
				creationForbidden = false;
				window()->cursor(FL_CURSOR_DEFAULT);
			}
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
			return 0; 
		}
		default:
//			return Fl_Widget::handle(event);
			return 0;
	}
}

void MyGrid::moving()
{
	Note* selected = &notes[selectedNote];

	float ex = Fl::event_x() - this->x();
	selected->col = (ex - movingGrabXOffset) / (float)colWidth;

	/* Ensure the note stays within X bounds */
	if (selected->col < 0.0)
		selected->col = 0.0;
	if (selected->col + selected->length > numCols)
		selected->col = numCols - selected->length;

	/* Apply snap */
	if (snap > 0.0)
		selected->col = std::round(selected->col / snap) * snap;

	float ey = Fl::event_y() - this->y();
	selected->row = (ey - movingGrabYOffset + rowHeight/2.0) / (float)rowHeight;

	/* Ensure the note stays within Y bounds */
	if (selected->row < 0)
		selected->row = 0;
	if (selected->row >= numRows)
		selected->row = numRows - 1;

//TODO find or implement a no-drop / not-allow / forbidden icon (circle with cross through it, or just X)
	amOverlapping = overlappingNote() >= 0;
	if (!amOverlapping)
		lastValidPosition = {selected->row, selected->col};
	if (amOverlapping)
		window()->cursor(forbiddenCursorImage(), 11, 11);
	else
		window()->cursor(FL_CURSOR_HAND);

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

	float ex = Fl::event_x() - this->x();

//TODO restrict length to a certain minimum
	if (side==LEFT) {
		float endCol = selected->col + selected->length;
		selected->col = ex / (float)colWidth;

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
		selected->length = ex / (float)colWidth - selected->col;

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

	float ex = Fl::event_x() - this->x();
	int ey = Fl::event_y() - this->y();

	int row = ey / rowHeight;
	float col = ex / colWidth;

	int selectedIfResize = 0;

	hoverState = NONE;

	for (const auto [i,n]: std::views::enumerate(notes)) {
		if (n.row != row) 
			continue;

		float leftEdge = n.col * colWidth; 
		float rightEdge = (n.col + n.length) * colWidth;

		if (leftEdge - ex <= resizeZone && ex - leftEdge <= resizeZone) {
			hoverState = RESIZING;
			side = LEFT;
			selectedIfResize = i;
		}
		else if (rightEdge - ex <= resizeZone && ex - rightEdge <= resizeZone) {
			hoverState = RESIZING;
			side = RIGHT;
			selectedIfResize = i;
		}
		else if (ex >= leftEdge && ex <= rightEdge) {
			//XXX only required on initial press down
			hoverState = MOVING;
			selectedNote = i;
			movingGrabXOffset = ex - n.col * colWidth;
			movingGrabYOffset = ey - n.row * rowHeight;
			originalPosition = {n.row,n.col};
			lastValidPosition = {n.row,n.col};

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

//TODO change to addNote() and deleteNote() I guess. Connect deleteNote() up to the menu
void MyGrid::toggleNote()
{
//TODO adjust for the position of the grid in the window. MAYBE use a "subwindow" so position starts at (0,0)
	int ex = Fl::event_x() - x();
	int ey = Fl::event_y() - y();

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
		bool clear = std::none_of(notes.begin(), notes.end(),
			[=](const Note& n) { return n.row == row && col < n.col + n.length && col + 1.0f > n.col; });
		if (clear)
			notes.push_back({row,col,1.0});
	}

	redraw();
}

void MyGrid::setTransport(ITransport* t, double b, int bpb) {
	Fl::remove_timeout(posTimerCb, this);
	transport   = t;
	bpm         = b;
	beatsPerBar = bpb;
	Fl::add_timeout(0.5, posTimerCb, this);
}

void MyGrid::posTimerCb(void* data) {
	auto* self = static_cast<MyGrid*>(data);
	self->checkAndRedraw();
	double interval = 0.1;  // slow poll when not playing
	if (self->transport && self->transport->isPlaying()) {
		double pxPerSec = (double)self->colWidth * self->bpm / 60.0 / self->beatsPerBar;
		interval = std::clamp(1.0 / pxPerSec, 0.016, 0.05);
	}
	Fl::repeat_timeout(interval, posTimerCb, data);
}

void MyGrid::checkAndRedraw() {
	if (transport && transport->isPlaying()) {
		double endSeconds = (double)numCols * beatsPerBar / bpm * 60.0;
		if (transport->position() >= endSeconds) {
			transport->pause();
			if (onEndReached) onEndReached();
		}
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

