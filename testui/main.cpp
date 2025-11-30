#include "FL/Enumerations.H"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>


class MyGrid : public Fl_Box {
	void draw() FL_OVERRIDE;
	int numRows;
	int numCols;
	int rowHeight;
	int colWidth;

public:
	MyGrid(int numRows,int numCols,int rowHeight,int colWidth): 
		numRows(numRows),numCols(numCols),rowHeight(rowHeight),colWidth(colWidth),
		Fl_Box(0,0,numCols * colWidth,numRows * rowHeight,nullptr) {}
};

void MyGrid::draw() {
	fl_color(0xEE888800);  //orange

	/* Rows: */
	for (int i = 0; i < numRows+1; i++) {
		int x0 = 0;
		int y0 = i * rowHeight;
		int x1 = numCols * colWidth;
		int y1 = y0; 

		fl_line(x0, y0, x1, y1);
	}

	fl_color(0x00EE0000); //green

	/* Columns: */
	for (int i = 0; i < numCols+1; i++) {
		int x0 = i * colWidth;
		int y0 = 0;
		int x1 = x0; 
		int y1 = numRows * rowHeight;

		fl_line(x0, y0, x1, y1);
	}
}


int main(int argc, char **argv) {
//XXX can we ditch the new?	
	Fl_Window *window = new Fl_Window(700, 600);
	window->color(0xF0F1F200);

	/*
	Fl_Box *box = new Fl_Box(20, 40, 300, 100,"blah");
	box->box(FL_UP_BOX);
	box->labelfont(FL_BOLD + FL_ITALIC);
	box->labelsize(36);
	box->labeltype(FL_SHADOW_LABEL);
	*/

	MyGrid p(10,20,30,30);
	p.box(FL_UP_BOX);
	p.align(FL_ALIGN_TOP);

	window->end();
	window->show(argc, argv);
	return Fl::run();
}

