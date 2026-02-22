#ifndef OUTER_GRID_HPP
#define OUTER_GRID_HPP

#include "FL/Fl_Group.H"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <vector>
#include "cell.hpp"
#include "grid.hpp"

/* The group is required so we can support multiple widgets. */
class OuterGrid : public Fl_Group {
	MyGrid grid;
//	int handle(int event) override;

public:
	OuterGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth,float snap);

	Fl_Menu_Button popup;
};

#endif

