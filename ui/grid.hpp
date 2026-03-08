#ifndef GRID_HPP
#define GRID_HPP

#include "popup.hpp"
#include "cell.hpp"
#include "observableTimeline.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>

class Playhead;

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
	float col;
} Point;


class MyGrid : public Fl_Box, public ITimelineObserver {
public:
	MyGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap,Popup& popup);
	~MyGrid();

	/* Grid parameters */
//XXX should be raised...
	int numRows;
	int numCols;

	int rowHeight;
	int colWidth;

private:
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
	bool creationForbidden;
	Point originalPosition;
	Point lastValidPosition;

	Playhead*           playhead          = nullptr;
	ObservableTimeline* timeline          = nullptr;
	int                 draggingPatternId = -1;
	float               originalLength    = 1.0f;
	bool                isDragging        = false;
	int                 trackFilter       = -1;   // -1 = all tracks
	bool                beatResolution    = false; // if true, cols are beats not bars

	void init();
	void draw() override;
	int  handle(int event) override;
	void findNoteForCursor();
	void toggleNote();
	int  overlappingNote();
	void moving();
	void resizing();
	void rebuildNotes();

public:
	void setPlayhead(Playhead* p) { playhead = p; }
	void setTimeline(ObservableTimeline* tl);
	void setTrackView(int trackFilter, bool beatResolution);
	void onTimelineChanged() override;
};

#endif
