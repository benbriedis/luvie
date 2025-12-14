#ifndef GRID_HPP
#define GRID_HPP

#include "popup.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>

enum SelectionState {
	NONE,
	MOVING,
	RESIZING
};

enum Side {
	LEFT,
	RIGHT,
};

/* XXX
   It would probably be best to have a single source of truth and store notes as beat and maybe pitch.
   Consider using a facade to map these to row, col and length - used here.
*/
typedef struct {
	int row;
	float col;  //XXX will want velocity + anything else? (parameters that can evolve etc)
	float length;
} Note;

typedef struct {
	int row;
	float col; 
} Point;


class MyGrid : public Fl_Box {
public:
	MyGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap,Popup& popup);


private:
	/* Grid parameters */
	int numRows;
	int numCols;
	int rowHeight;
	int colWidth;
	float snap;
	Popup& popup;

	/* Note parameters: */
	std::vector<Note> notes;

	/* Cursor parameters: */
	/* selectedNote points to 'notes'. This is a Vector and can be reallocated - so using a pointer is not safe. */
	int selectedNote; 
	SelectionState hoverState;
	Side side;
	float movingGrabXOffset;
	float movingGrabYOffset;
	bool amOverlapping;
	Point pickupPoint;

	void init();
	void draw() override;
	int handle(int event) override;
	void findNoteForCursor();
	void toggleNote();
	int overlappingNote();
	void moving();
	void resizing();
};

#endif

