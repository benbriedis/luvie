#ifndef GRID_HPP
#define GRID_HPP

#include <FL/Fl_Box.H>


class MyGrid : public Fl_Box {
	void draw() FL_OVERRIDE;
	int numRows;
	int numCols;
	int rowHeight;
	int colWidth;

public:
	MyGrid(int numRows,int numCols,int rowHeight,int colWidth);
};

#endif
