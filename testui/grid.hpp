#ifndef GRID_HPP
#define GRID_HPP

#include <FL/Fl_Box.H>

enum SelectionState {
	NONE,
	MOVING,
	RESIZING
};

enum Side {
	LEFT,
	RIGHT,
};

typedef struct {
	int row;
	float beat;  //XXX will want velocity + anything else? (parameters that can evolve etc)
	float length;
} Note;

typedef struct {
	int row;
	float beat; 
} Point;


class MyGrid : public Fl_Box {
public:
	MyGrid(std::vector<Note> notes,int numRows,int numBeats,int rowHeight,int colWidth);

private:
	/* Grid parameters */
	int numRows;
	int numBeats;
	int rowHeight;
	int colWidth;

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

	void draw() override;
	int handle(int event) override;
	void findNoteForCursor();
	void toggleNote();
	int overlappingNote();
	void moving();
	void resizing();
};

#endif
