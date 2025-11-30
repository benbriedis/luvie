#ifndef GRID_HPP
#define GRID_HPP

#include <FL/Fl_Box.H>


typedef struct {
	int row;
	int col;  //XXX will want fractions + length + velocity + anything else? (parameters that can evolve etc)
} Note;


class MyGrid : public Fl_Box {
public:
	MyGrid(std::vector<Note> notes,int numRows,int numCols,int rowHeight,int colWidth);

private:
	std::vector<Note> notes;
	int numRows;
	int numCols;
	int rowHeight;
	int colWidth;

	void draw() override;
	int handle(int event) override;
	void toggleNote();
};

#endif
