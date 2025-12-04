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
	SelectionState hoverState;
	Note* selectedNote; 
	Side side;
	float movingGrabOffset;

	void draw() override;
	int handle(int event) override;
	void findNoteForCursor();
	void toggleNote();
};

#endif
